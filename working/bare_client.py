import asyncio
import http.cookies
import logging
import secrets
import urllib.parse
from typing import Any

import aiohttp
import zlib

import steam_crypto
import steamid
from parsers.kv import KeyValues
from steamid import SteamID

USER_AGENT = 'Valve/Steam HTTP Client 1.0'
HOSTNAME = 'api.steampowered.com'


# TODO: prefer composition over inheritance, prefer DI

class SteamUserWebAPI:
    def __init__(self, hostname: str, client: aiohttp.ClientSession,
                 *, logger: logging.Logger = None, **options):
        self.hostname = hostname
        self.client = client
        self.options = options
        self.logger = logger or logging.Logger(self.__class__.__name__)

    async def _api_request(self, http_method: str, iface: str, method: str, version, data: dict):
        data = data or {}
        http_method = http_method.upper()
        version = str(version).rjust(4, '0')
        data['format'] = 'vdf'

        # query = urllib.parse.urlencode(data)
        query = build_query_string(data)
        headers = _get_default_headers()
        headers.update(self.options.get('additional_headers', {}))
        path = f'/{iface}/{method}/v{version}/'

        if http_method == 'POST':
            headers['Content-Type'] = 'application/x-www-form-urlencoded'
            headers['Content-Length'] = len(query)
        else:
            path += '?' + query

        uri = f'https://{self.hostname}{path}'  # TODO: yarl.URL
        # TODO: set options.localAddress
        # TODO: set options.proxyAgent

        # TODO: handle streamed response
        response = await self.client.request(
            http_method, uri, headers=headers,
            raise_for_status=True,
        )
        response_data = await response.read()

        self.logger.debug('API %s response to %s: %s', http_method, uri, response.status)
        # if response.status != 200:
        #     raise HTTPError(response.status)

        # try:
        #     return VDF.parse(response)
        # except Exception as e:
        #     raise e

        if response.headers.get('content-encoding', '').lower() == 'gzip':
            response_data = zlib.decompress(response_data)

        return dict(KeyValues(response_data.decode('utf-8')))


class SteamUserWeb(SteamUserWebAPI):
    def __init__(
            self, hostname: str, client: aiohttp.ClientSession,
            steam_id: SteamID,
            **options):
        super().__init__(hostname, client, **options)
        self.steam_id = steam_id

    def web_log_on(self):
        if not self.steam_id:
            raise ValueError("Cannot log onto steamcommunity.com without first being connected to Steam network")
        if self.steam_id.type != steamid.Type.INDIVIDUAL:
            raise ValueError("Must not be anonymous user to use webLogOn (check to see you passed in valid "
                             "credentials to logOn)")

        # TODO: inject SteamUserMessages
        steam_user_messages.send(EMsg.ClientRequestWebAPIAuthenticateUserNonce, {})

    def _web_log_on(self):
        if not self.steam_id or self.steam_id.type != steamid.Type.INDIVIDUAL:
            return
        self.web_log_on()

    async def _web_authenticate(self, nonce: bytes):
        session_key = steam_crypto.generate_session_key(nonce)  # NOTE: nonce not passed in reference implementation
        encrypted_nonce = steam_crypto.symmetric_encrypt_with_hmac_iv(nonce, session_key.plain)

        data = dict(
            steam_id=str(self.steam_id),
            session_key=session_key.encrypted,
            encrypted_loginkey=encrypted_nonce,
        )

        session_id: str
        cookies = http.cookies.SimpleCookie()  # TODO: stored cookies aren't used?

        try:
            res = self._api_request('POST', 'ISteamUserAuth', 'AuthenticateUser', 1, data)
            if (auth := res.get('authenticateuser')) is None or (not auth.get('token') and not auth.get('tokensecure')):  # TODO: verify condition, is 'not' outside instead?
                raise Exception("Malformed response")
            session_id = secrets.token_hex(12)
            cookies['sessionid'] = session_id
            if token := auth.get('token'):
                cookies['steamLogin'] = token
            if token_secure := auth.get('tokensecure'):
                cookies['steamLoginSecure'] = token_secure
        except aiohttp.ClientResponseError as ex:
            self.logger.debug('Webauth failed', exc_info=ex)
            # TODO: use rate limiter context manager instead
            if ex.status == 429:
                # rate-limited
                self._webauthTimeout = 5e4
            if self._webauthTimeout:
                self._webauthTimeout = min(self._webauthTimeout * 2, 50000)
            else:
                self._webauthTimeout = 1000

        with asyncio.Timeout(0.5):
            await self._web_log_on()

    async def handler_auth_loop(self, body):
        if body.eresult != EResult.OK:
            self.logger.debug('Got response %s from ClientRequestWebAPIAuthenticateUserNonceResponse, retrying', body.eresult)
            with asyncio.Timeout(0.5):
                await self._web_log_on()
        else:
            self._web_authenticate()

    handlers = {}
    handlers[EMsg.ClientRequestWebAPIAuthenticateUserNonceResponse] = handler_auth_loop

def build_query_string(data: dict[str, Any]):
    query = ''
    for i in data:
        query += ('&' if query else '') + i + '='
        d = data[i]
        if isinstance(d, bytes):
            # TODO: check validity
            w = d.hex().upper()
            query += "".join("%2s" % w[i:i + 2] for i in range(0, len(w), 2))
        else:
            query += urllib.parse.quote(data[i])
    return query


def _get_default_headers():
    return {
        'Accept': 'text/html,*/*;q=0.9',
        'Accept-Encoding': 'gzip,identity,*;q=0',
        'Accept-Charset': 'ISO-8859-1,utf-8,*;q=0.7',
        'User-Agent': USER_AGENT
    }


async def main():
    # node-steam-user/components/06-webapi.js
    async with aiohttp.ClientSession() as session:
        client = SteamUserWeb(HOSTNAME, session, SteamID(76561198140075408))
        client.web_log_on()


if __name__ == "__main__":
    asyncio.run(main())
