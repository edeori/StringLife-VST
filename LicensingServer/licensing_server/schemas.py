from __future__ import annotations

from typing import Optional

from pydantic import BaseModel


class CreateLicenseRequest(BaseModel):
    licenseKey: str = ""
    productId: str
    maxActivations: int = 2
    comment: str = ""


class CreateProductRequest(BaseModel):
    productId: str
    displayName: str


class LicenseRequest(BaseModel):
    licenseKey: str
    productId: str
    machineId: str
    pluginVersion: str
    platform: str
    activationId: Optional[str] = None
    signedLicense: Optional[str] = None


class LicenseResponse(BaseModel):
    success: bool
    status: str
    activationId: str = ""
    signedLicense: str = ""
    expiresAt: Optional[str] = None
    maxActivations: int = 0
    message: str


class AdminLicenseView(BaseModel):
    licenseKey: str
    productId: str
    maxActivations: int
    isEnabled: bool
    isRevoked: bool
    comment: str
    expiresAt: Optional[str]
    createdAt: str
    activeActivationCount: int


class AdminProductView(BaseModel):
    productId: str
    displayName: str
    isEnabled: bool
    createdAt: str
    totalLicenses: int
    activeActivations: int


class AdminActivationView(BaseModel):
    activationId: str
    licenseKey: str
    productId: str
    machineId: str
    platform: str
    pluginVersion: str
    activatedAt: str
    lastValidatedAt: str
    deactivatedAt: Optional[str]


class AdminOverviewResponse(BaseModel):
    selectedProductId: str
    availableProductIds: list[str]
    products: list[AdminProductView]
    licenses: list[AdminLicenseView]
    activations: list[AdminActivationView]
