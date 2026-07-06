from __future__ import annotations

import hashlib
import json


def _parse_juce_rsa_key(key_text: str) -> tuple[int, int]:
    cleaned = key_text.strip()

    if not cleaned:
        raise ValueError("Missing RSA key")

    parts = [part.strip() for part in cleaned.split(",", 1)]

    if len(parts) != 2 or not parts[0] or not parts[1]:
        raise ValueError("JUCE RSA key must be '<hex exponent>,<hex modulus>'")

    return int(parts[0], 16), int(parts[1], 16)


def sign_payload(payload: dict[str, object], private_key_text: str, *, key_id: str = "default") -> str:
    exponent, modulus = _parse_juce_rsa_key(private_key_text)
    payload_text = json.dumps(payload, separators=(",", ":"), sort_keys=True)
    digest = hashlib.sha256(payload_text.encode("utf-8")).digest()
    digest_value = int.from_bytes(digest, byteorder="big", signed=False)

    if digest_value >= modulus:
        raise ValueError("RSA modulus is too small for SHA-256 signing")

    signature_value = pow(digest_value, exponent, modulus)
    signature_hex = format(signature_value, "x")

    envelope = {
        "algorithm": "rsa-sha256",
        "keyId": key_id,
        "payload": payload_text,
        "signature": signature_hex,
    }
    return json.dumps(envelope, separators=(",", ":"), sort_keys=True)
