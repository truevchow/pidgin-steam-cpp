# node-steam-user/index.js
import enum
import re


class Universe(enum.Enum):
    INVALID = 0
    PUBLIC = 1
    BETA = 2
    INTERNAL = 3
    DEV = 4


class Type(enum.Enum):
    INVALID = 0
    INDIVIDUAL = 1
    MULTISEAT = 2
    GAMESERVER = 3
    ANON_GAMESERVER = 4
    PENDING = 5
    CONTENT_SERVER = 6
    CLAN = 7
    CHAT = 8
    P2P_SUPER_SEEDER = 9
    ANON_USER = 10


class Instance(enum.Enum):
    ALL = 0
    DESKTOP = 1
    CONSOLE = 2
    WEB = 4


TypeChars = {
    Type.INVALID: 'I',
    Type.INDIVIDUAL: 'U',
    Type.MULTISEAT: 'M',
    Type.GAMESERVER: 'G',
    Type.ANON_GAMESERVER: 'A',
    Type.PENDING: 'P',
    Type.CONTENT_SERVER: 'C',
    Type.CLAN: 'g',
    Type.CHAT: 'T',
    Type.ANON_USER: 'a'
}

CharsType: dict[str, Type] = {v: k for k, v in TypeChars.items()}

AccountIDMask = 0xFFFFFFFF
AccountInstanceMask = 0x000FFFFF


class ChatInstanceFlags(enum.Enum):
    Clan = (AccountInstanceMask + 1) >> 1
    Lobby = (AccountInstanceMask + 1) >> 2
    MMSLobby = (AccountInstanceMask + 1) >> 3


class SteamID:
    def __init__(self, input: str | int):
        self.universe: Universe = Universe.INVALID
        self.type: Type = Type.INVALID
        self.instance: Instance = Instance.ALL
        self.account_id: int = 0

        if not input:
            return

        try:
            input = int(input)
        except ValueError:
            pass

        if isinstance(input, int):
            self.account_id = input & AccountIDMask
            self.instance = Instance((input >> 32) & AccountInstanceMask)
            self.type = Type((input >> 52) & 0xF)
            self.universe = Universe(input >> 56)
        elif ma := re.match(r"^STEAM_([0-5]):([0-1]):([0-9]+)$", input):
            # Steam2 ID
            universe, mod, account_id = ma.groups()
            self.universe = Universe(int(universe) or 1)
            self.type = Type.INDIVIDUAL
            self.instance = Instance.DESKTOP
            self.account_id = int(account_id) * 2 + mod
        elif ma := re.match(r"^\[([a-zA-Z]):([0-5]):([0-9]+)(:[0-9]+)?]$", input):
            # Steam3 ID
            type_char, universe, account_id, instance_id = ma.groups()
            self.universe = Universe(int(universe))
            self.account_id = int(account_id)

            if instance_id:
                self.instance = instance_id[1:]

            # TODO: use bidict
            match type_char:
                case 'U':
                    self.type = Type.INDIVIDUAL
                    if not instance_id:
                        self.instance = Instance.DESKTOP
                case 'c':
                    self.instance = ChatInstanceFlags.Clan
                    self.type = Type.CHAT
                case 'L':
                    self.instance = ChatInstanceFlags.Lobby
                    self.type = Type.CHAT
                case _:
                    self.type: Type = CharsType.get(type_char, Type.INVALID)

            # self.type = Type.INDIVIDUAL
            # self.instance = Instance.DESKTOP
        else:
            raise ValueError(input)

    def from_individual_account_id(self, account_id: int) -> 'SteamID':
        raise NotImplementedError

    def is_valid(self) -> bool:
        if self.type is Type.INVALID or self.universe is Universe.INVALID:
            return False
        if self.type is Type.INDIVIDUAL and self.account_id == 0:
            return False
        if self.type is Type.CLAN and (self.account_id == 0 or self.instance != Instance.ALL):
            return False
        if self.type is Type.GAMESERVER and self.account_id == 0:
            return False
        return True

    def is_valid_individual(self):
        return self.universe == Universe.PUBLIC and self.type == Type.INDIVIDUAL and self.instance == Instance.DESKTOP and self.is_valid()

    def is_group_chat(self):
        return self.type == Type.CHAT and (self.instance.value & ChatInstanceFlags.Clan.value) != 0

    def is_lobby(self):
        return self.type == Type.CHAT and \
            ((self.instance.value & ChatInstanceFlags.Lobby.value) != 0 or
             (self.instance.value & ChatInstanceFlags.MMSLobby.value) != 0)

    def steam2(self, newer_format: bool = False) -> str:
        if self.type != Type.INDIVIDUAL:
            raise ValueError("Can't get Steam2 rendered ID for non-individual ID")

        universe = self.universe
        if not newer_format and universe is Universe.PUBLIC:
            universe = Universe.INVALID
        return f"STEAM_{universe}:{self.account_id & 1}:{self.account_id // 2}"

    def steam2_rendered(self, newer_format: bool = False) -> str:
        return self.steam2(newer_format)

    def steam3(self) -> str:
        raise NotImplementedError

    def steam3_rendered(self) -> str:
        return self.steam3()

    def steamid64(self) -> str:
        return str(self.int_id())

    def __str__(self):
        return self.steamid64()

    def int_id(self) -> int:
        return (self.universe.value << 56) | (self.type.value << 52) | (self.instance.value << 32) | self.account_id