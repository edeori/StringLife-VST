#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

from cryptography.hazmat.primitives.asymmetric import rsa


def to_juce_key(exponent: int, modulus: int) -> str:
    return f"{exponent:x},{modulus:x}"


def main() -> None:
    private_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    private_numbers = private_key.private_numbers()
    public_numbers = private_numbers.public_numbers

    juce_public = to_juce_key(public_numbers.e, public_numbers.n)
    juce_private = to_juce_key(private_numbers.d, public_numbers.n)

    keys_dir = Path(__file__).resolve().parents[1] / "keys"
    keys_dir.mkdir(parents=True, exist_ok=True)

    (keys_dir / "public_key.txt").write_text(juce_public + "\n")
    (keys_dir / "private_key.txt").write_text(juce_private + "\n")
    (keys_dir / "README.txt").write_text(
        "public_key.txt -> copy into plugin licensing_config.json publicKey\n"
        "private_key.txt -> copy into LICENSING_SIGNING_PRIVATE_KEY in server .env\n"
        "Do not commit these files.\n"
    )

    print("Public key for plugin config (publicKey):")
    print(juce_public)
    print()
    print("Private key for licensing server env (LICENSING_SIGNING_PRIVATE_KEY):")
    print(juce_private)
    print()
    print(f"Saved to: {keys_dir}")
    print()
    print("Example:")
    print(f'LICENSING_SIGNING_PRIVATE_KEY="{juce_private}"')


if __name__ == "__main__":
    main()
