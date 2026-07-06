from __future__ import annotations

import json
import uuid
from datetime import datetime, timezone

from sqlmodel import Session, select

from .config import settings
from .models import ActivationRecord, LicenseRecord, ProductRecord
from .schemas import (
    AdminActivationView,
    AdminLicenseView,
    AdminOverviewResponse,
    AdminProductView,
    CreateLicenseRequest,
    CreateProductRequest,
    LicenseRequest,
    LicenseResponse,
)
from .signing import sign_payload


def _utc_iso(value: datetime | None) -> str | None:
    if value is None:
        return None

    return value.astimezone(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def _signed_license_payload(request: LicenseRequest, license_record: LicenseRecord, activation: ActivationRecord) -> str:
    payload = {
        "licenseKey": request.licenseKey,
        "productId": request.productId,
        "machineId": request.machineId,
        "activationId": activation.activation_id,
        "status": "activated",
        "issuedAt": _utc_iso(activation.activated_at),
        "lastValidatedAt": _utc_iso(activation.last_validated_at),
        "offlineGraceDays": settings.default_offline_grace_days,
        "expiresAt": _utc_iso(license_record.expires_at),
        "maxActivations": license_record.max_activations,
        "message": "License active",
    }

    if settings.signing_private_key:
        return sign_payload(payload, settings.signing_private_key, key_id=settings.signing_key_id)

    if settings.allow_unsigned_development_licenses:
        envelope = {
            "algorithm": "none",
            "keyId": "development",
            "payload": json.dumps(payload, separators=(",", ":"), sort_keys=True),
            "signature": "",
        }
        return json.dumps(envelope, separators=(",", ":"), sort_keys=True)

    raise RuntimeError("Licensing server signing is not configured")


def _error(status: str, message: str, *, max_activations: int = 0) -> LicenseResponse:
    return LicenseResponse(
        success=False,
        status=status,
        message=message,
        maxActivations=max_activations,
    )


def _generate_license_key() -> str:
    raw = uuid.uuid4().hex.upper()
    return f"LIC-{raw[0:8]}-{raw[8:12]}-{raw[12:16]}-{raw[16:20]}-{raw[20:32]}"


def _find_license(session: Session, request: LicenseRequest) -> LicenseRecord | None:
    statement = select(LicenseRecord).where(
        LicenseRecord.license_key == request.licenseKey,
        LicenseRecord.product_id == request.productId,
    )
    return session.exec(statement).first()


def _find_active_activation(session: Session, request: LicenseRequest) -> ActivationRecord | None:
    statement = select(ActivationRecord).where(
        ActivationRecord.license_key == request.licenseKey,
        ActivationRecord.product_id == request.productId,
        ActivationRecord.machine_id == request.machineId,
        ActivationRecord.deactivated_at.is_(None),
    )
    return session.exec(statement).first()


def _count_active_activations(session: Session, license_key: str, product_id: str) -> int:
    statement = select(ActivationRecord).where(
        ActivationRecord.license_key == license_key,
        ActivationRecord.product_id == product_id,
        ActivationRecord.deactivated_at.is_(None),
    )
    return len(list(session.exec(statement)))


def activate_license(session: Session, request: LicenseRequest) -> LicenseResponse:
    license_record = _find_license(session, request)

    product_record = session.exec(select(ProductRecord).where(ProductRecord.product_id == request.productId)).first()

    if product_record is None or not product_record.is_enabled:
        return _error("invalid_license", "Product is disabled or unknown")

    if license_record is None:
        return _error("invalid_license", "License key not found")

    if not license_record.is_enabled or license_record.is_revoked:
        return _error("invalid_license", "License is disabled", max_activations=license_record.max_activations)

    if license_record.expires_at and license_record.expires_at < datetime.now(timezone.utc):
        return _error("expired_license", "License expired", max_activations=license_record.max_activations)

    existing_activation = _find_active_activation(session, request)

    if existing_activation is not None:
        existing_activation.last_validated_at = datetime.now(timezone.utc)
        session.add(existing_activation)
        session.commit()
        session.refresh(existing_activation)
        return LicenseResponse(
            success=True,
            status="activated",
            activationId=existing_activation.activation_id,
            signedLicense=_signed_license_payload(request, license_record, existing_activation),
            expiresAt=_utc_iso(license_record.expires_at),
            maxActivations=license_record.max_activations,
            message="License already active on this machine",
        )

    if _count_active_activations(session, request.licenseKey, request.productId) >= license_record.max_activations:
        return _error("invalid_license", "Maximum activations reached", max_activations=license_record.max_activations)

    activation = ActivationRecord(
        license_key=request.licenseKey,
        product_id=request.productId,
        machine_id=request.machineId,
        platform=request.platform,
        plugin_version=request.pluginVersion,
    )
    session.add(activation)
    session.commit()
    session.refresh(activation)

    return LicenseResponse(
        success=True,
        status="activated",
        activationId=activation.activation_id,
        signedLicense=_signed_license_payload(request, license_record, activation),
        expiresAt=_utc_iso(license_record.expires_at),
        maxActivations=license_record.max_activations,
        message="Activation successful",
    )


def validate_license(session: Session, request: LicenseRequest) -> LicenseResponse:
    license_record = _find_license(session, request)

    product_record = session.exec(select(ProductRecord).where(ProductRecord.product_id == request.productId)).first()

    if product_record is None or not product_record.is_enabled:
        return _error("invalid_license", "Product is disabled or unknown")

    if license_record is None:
        return _error("invalid_license", "License key not found")

    if not license_record.is_enabled or license_record.is_revoked:
        return _error("invalid_license", "License is disabled", max_activations=license_record.max_activations)

    if license_record.expires_at and license_record.expires_at < datetime.now(timezone.utc):
        return _error("expired_license", "License expired", max_activations=license_record.max_activations)

    activation = _find_active_activation(session, request)

    if activation is None:
        return _error("activation_required", "Machine is not activated", max_activations=license_record.max_activations)

    if request.activationId and request.activationId != activation.activation_id:
        return _error("invalid_license", "Activation ID does not match this machine", max_activations=license_record.max_activations)

    activation.last_validated_at = datetime.now(timezone.utc)
    session.add(activation)
    session.commit()
    session.refresh(activation)

    return LicenseResponse(
        success=True,
        status="activated",
        activationId=activation.activation_id,
        signedLicense=_signed_license_payload(request, license_record, activation),
        expiresAt=_utc_iso(license_record.expires_at),
        maxActivations=license_record.max_activations,
        message="Validation successful",
    )


def refresh_license(session: Session, request: LicenseRequest) -> LicenseResponse:
    response = validate_license(session, request)

    if response.success:
        response.message = "Refresh successful"

    return response


def deactivate_license(session: Session, request: LicenseRequest) -> LicenseResponse:
    license_record = _find_license(session, request)

    if license_record is None:
        return _error("invalid_license", "License key not found")

    activation = _find_active_activation(session, request)

    if activation is None:
        return _error("activation_required", "No active activation found", max_activations=license_record.max_activations)

    activation.deactivated_at = datetime.now(timezone.utc)
    activation.last_validated_at = datetime.now(timezone.utc)
    session.add(activation)
    session.commit()

    return LicenseResponse(
        success=True,
        status="deactivated",
        activationId=activation.activation_id,
        signedLicense="",
        expiresAt=_utc_iso(license_record.expires_at),
        maxActivations=license_record.max_activations,
        message="Deactivation successful",
    )


def create_license_record(session: Session, request: CreateLicenseRequest) -> LicenseRecord:
    requested_key = request.licenseKey.strip()

    if requested_key:
        existing = session.exec(
            select(LicenseRecord).where(
                LicenseRecord.license_key == requested_key,
                LicenseRecord.product_id == request.productId,
            )
        ).first()

        if existing is not None:
            return existing

    if session.exec(select(ProductRecord).where(ProductRecord.product_id == request.productId)).first() is None:
        raise ValueError(f"Unknown product: {request.productId}")

    license_key = requested_key or _generate_license_key()

    existing = session.exec(
        select(LicenseRecord).where(
            LicenseRecord.license_key == license_key,
            LicenseRecord.product_id == request.productId,
        )
    ).first()

    if existing is not None:
        return existing

    record = LicenseRecord(
        license_key=license_key,
        product_id=request.productId,
        max_activations=request.maxActivations,
        comment=request.comment.strip(),
        is_enabled=True,
        is_revoked=False,
    )
    session.add(record)
    session.commit()
    session.refresh(record)
    return record


def create_product_record(session: Session, request: CreateProductRequest) -> ProductRecord:
    product_id = request.productId.strip()
    display_name = request.displayName.strip()

    existing = session.exec(select(ProductRecord).where(ProductRecord.product_id == product_id)).first()

    if existing is not None:
        return existing

    record = ProductRecord(product_id=product_id, display_name=display_name, is_enabled=True)
    session.add(record)
    session.commit()
    session.refresh(record)
    return record


def delete_product_record(session: Session, product_id: str) -> bool:
    product = session.exec(select(ProductRecord).where(ProductRecord.product_id == product_id)).first()

    if product is None:
        return False
    product_activations = session.exec(select(ActivationRecord).where(ActivationRecord.product_id == product_id)).all()
    product_licenses = session.exec(select(LicenseRecord).where(LicenseRecord.product_id == product_id)).all()

    for activation in product_activations:
        session.delete(activation)

    for license_record in product_licenses:
        session.delete(license_record)

    session.delete(product)
    session.commit()
    return True


def get_known_product_ids(session: Session) -> list[str]:
    return sorted(record.product_id for record in session.exec(select(ProductRecord)).all())


def set_license_max_activations(session: Session, license_key: str, product_id: str, value: int) -> None:
    record = session.exec(
        select(LicenseRecord).where(
            LicenseRecord.license_key == license_key,
            LicenseRecord.product_id == product_id,
        )
    ).first()

    if record is None:
        return

    active_activation_count = _count_active_activations(session, license_key, product_id)
    record.max_activations = max(1, active_activation_count, value)
    session.add(record)
    session.commit()


def set_license_comment(session: Session, license_key: str, product_id: str, comment: str) -> None:
    record = session.exec(
        select(LicenseRecord).where(
            LicenseRecord.license_key == license_key,
            LicenseRecord.product_id == product_id,
        )
    ).first()

    if record is None:
        return

    record.comment = comment.strip()
    session.add(record)
    session.commit()


def toggle_license_enabled(session: Session, license_key: str, product_id: str) -> None:
    record = session.exec(
        select(LicenseRecord).where(
            LicenseRecord.license_key == license_key,
            LicenseRecord.product_id == product_id,
        )
    ).first()

    if record is None:
        return

    record.is_enabled = not record.is_enabled
    session.add(record)
    session.commit()


def delete_license_record(session: Session, license_key: str, product_id: str) -> None:
    activations = session.exec(
        select(ActivationRecord).where(
            ActivationRecord.license_key == license_key,
            ActivationRecord.product_id == product_id,
        )
    ).all()
    record = session.exec(
        select(LicenseRecord).where(
            LicenseRecord.license_key == license_key,
            LicenseRecord.product_id == product_id,
        )
    ).first()

    if record is None:
        return

    for activation in activations:
        session.delete(activation)

    session.delete(record)
    session.commit()


def get_admin_overview(session: Session, product_id: str = "") -> AdminOverviewResponse:
    products = list(session.exec(select(ProductRecord).order_by(ProductRecord.display_name.asc())))
    licenses = list(session.exec(select(LicenseRecord).order_by(LicenseRecord.created_at.desc())))
    activations = list(session.exec(select(ActivationRecord).order_by(ActivationRecord.activated_at.desc())))
    available_product_ids = get_known_product_ids(session)

    if product_id:
        licenses = [record for record in licenses if record.product_id == product_id]
        activations = [record for record in activations if record.product_id == product_id]

    license_views: list[AdminLicenseView] = []

    for license_record in licenses:
        active_count = len(
            [
                activation
                for activation in activations
                if activation.license_key == license_record.license_key
                and activation.product_id == license_record.product_id
                and activation.deactivated_at is None
            ]
        )
        license_views.append(
            AdminLicenseView(
                licenseKey=license_record.license_key,
                productId=license_record.product_id,
                maxActivations=license_record.max_activations,
                isEnabled=license_record.is_enabled,
                isRevoked=license_record.is_revoked,
                comment=license_record.comment,
                expiresAt=_utc_iso(license_record.expires_at),
                createdAt=_utc_iso(license_record.created_at) or "",
                activeActivationCount=active_count,
            )
        )

    activation_views = [
        AdminActivationView(
            activationId=activation.activation_id,
            licenseKey=activation.license_key,
            productId=activation.product_id,
            machineId=activation.machine_id,
            platform=activation.platform,
            pluginVersion=activation.plugin_version,
            activatedAt=_utc_iso(activation.activated_at) or "",
            lastValidatedAt=_utc_iso(activation.last_validated_at) or "",
            deactivatedAt=_utc_iso(activation.deactivated_at),
        )
        for activation in activations
    ]

    product_views = []

    for product in products:
        product_license_records = [license for license in licenses if license.product_id == product.product_id] if product_id else [
            license for license in session.exec(select(LicenseRecord).where(LicenseRecord.product_id == product.product_id)).all()
        ]
        product_activations = [activation for activation in activations if activation.product_id == product.product_id] if product_id else [
            activation for activation in session.exec(select(ActivationRecord).where(ActivationRecord.product_id == product.product_id)).all()
        ]

        product_views.append(
            AdminProductView(
                productId=product.product_id,
                displayName=product.display_name,
                isEnabled=product.is_enabled,
                createdAt=_utc_iso(product.created_at) or "",
                totalLicenses=len(product_license_records),
                activeActivations=len([activation for activation in product_activations if activation.deactivated_at is None]),
            )
        )

    return AdminOverviewResponse(
        selectedProductId=product_id,
        availableProductIds=available_product_ids,
        products=product_views,
        licenses=license_views,
        activations=activation_views,
    )
