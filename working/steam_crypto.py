# node-steam-crypto/index.js

import secrets
from pathlib import Path
from typing import NamedTuple

from cryptography.hazmat.primitives import hashes
# from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives.hmac import HMAC
from cryptography.hazmat.primitives.serialization import load_pem_public_key


def load_steam_pubkey():
    path = Path(__file__).parents[1] / "./third_party_ref/node-steam-crypto/system.pem"
    path = path.resolve()
    assert path.is_file(), path
    return load_pem_public_key(path.read_bytes())


steam_pubkey = load_steam_pubkey()


def verify_signature(data: bytes, signature: bytes, algorithm):
    steam_pubkey.verify(signature, data, algorithm)


class _SessionKeyResult(NamedTuple):
    plain: bytes
    encrypted: bytes


def generate_session_key(nonce: bytes):
    session_key = secrets.token_bytes(32)
    encrypted_session_key = steam_pubkey.encrypt(b"".join([session_key, nonce or b""]))
    return _SessionKeyResult(
        plain=session_key,
        encrypted=encrypted_session_key,
    )


def symmetric_encrypt(plain: bytes, key: bytes, iv: bytes) -> bytes:
    if iv is None:
        iv = secrets.token_bytes(16)

    encryptor = Cipher(algorithms.AES(key), modes.ECB()).encryptor()
    aes_iv = encryptor.update(iv) + encryptor.finalize()

    encryptor = Cipher(algorithms.AES(key), modes.CBC(iv)).encryptor()
    aes_data = encryptor.update(plain) + encryptor.finalize()

    return aes_iv + aes_data


def symmetric_encrypt_with_hmac_iv(plain: bytes, key: bytes) -> bytes:
    u = secrets.token_bytes(3)
    h = HMAC(key[:16], hashes.SHA1())
    h.update(u)
    h.update(plain)
    return symmetric_encrypt(plain, key, h.finalize()[:16 - len(u)] + u)


def symmetric_decrypt(enc: bytes, key: bytes, check_hmac: bool) -> bytes:
    # TODO: verify padding disabled!
    decryptor = Cipher(algorithms.AES(key), modes.ECB()).decryptor()
    iv = decryptor.update(enc[:16]) + decryptor.finalize()

    decryptor = Cipher(algorithms.AES(key), modes.CBC(iv)).decryptor()
    plain = decryptor.update(enc[16:]) + decryptor.finalize()

    if check_hmac:
        remote_partial_hmac = iv[:len(iv) - 3]
        u = iv[-3:]
        h = HMAC(key[:16], hashes.SHA1())
        h.update(u)
        h.update(plain)
        if remote_partial_hmac != h.finalize()[:len(remote_partial_hmac)]:
            raise ValueError("Received invalid HMAC from remote host")

    return plain


def symmetric_decrypt_ecb(enc: bytes, key: bytes) -> bytes:
    decryptor = Cipher(algorithms.AES(key), modes.ECB()).decryptor()
    return decryptor.update(enc) + decryptor.finalize()
