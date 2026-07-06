from __future__ import annotations

from datetime import datetime, timezone
from typing import Optional
from uuid import uuid4

from sqlmodel import Field, SQLModel


def utcnow() -> datetime:
    return datetime.now(timezone.utc)


class LicenseRecord(SQLModel, table=True):
    id: Optional[int] = Field(default=None, primary_key=True)
    license_key: str = Field(index=True, unique=True)
    product_id: str = Field(index=True)
    max_activations: int = 2
    comment: str = ""
    is_enabled: bool = True
    is_revoked: bool = False
    expires_at: Optional[datetime] = None
    created_at: datetime = Field(default_factory=utcnow)


class ProductRecord(SQLModel, table=True):
    id: Optional[int] = Field(default=None, primary_key=True)
    product_id: str = Field(index=True, unique=True)
    display_name: str
    is_enabled: bool = True
    created_at: datetime = Field(default_factory=utcnow)


class ActivationRecord(SQLModel, table=True):
    id: Optional[int] = Field(default=None, primary_key=True)
    activation_id: str = Field(default_factory=lambda: str(uuid4()), index=True, unique=True)
    license_key: str = Field(index=True)
    product_id: str = Field(index=True)
    machine_id: str = Field(index=True)
    platform: str
    plugin_version: str
    activated_at: datetime = Field(default_factory=utcnow)
    last_validated_at: datetime = Field(default_factory=utcnow)
    deactivated_at: Optional[datetime] = None
