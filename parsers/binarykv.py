# node-binarykvparser/index.js
import enum
import io
import struct
from typing import Union


class Type(enum.Enum):
    NONE = 0
    String = 1
    Int32 = 2
    Float32 = 3
    Pointer = 4
    WideString = 5
    Color = 6
    UInt64 = 7
    Int64 = 10  # signed
    End = 8


def convert_object(obj: dict) -> dict | list:
    """
    Converts an object to an array if it's an array-like object
    :param obj:
    :return: object or array
    """

    # TODO: when is 'obj' equivalent to JavaScript 'object'?
    keys = list(obj.keys())
    res = {}
    for i, k in enumerate(keys):
        try:
            k = keys[i] = int(k)
        except ValueError:
            return obj
        res[k] = obj[k]

    if sorted(keys) != range(len(keys)):
        return obj

    return [res[i] for i in range(len(keys))]


class Parser:
    def __init__(self, buffer: Union[bytes, io.BytesIO], offset: int = None):
        if isinstance(buffer, bytes):
            contents = buffer
            buffer = io.BytesIO(buffer)
            offset = offset or 0
        elif isinstance(buffer, io.BytesIO):
            contents = buffer.getvalue()
            offset = buffer.tell()
        else:
            raise TypeError(buffer)

        assert isinstance(contents, bytes)
        assert isinstance(buffer, io.BytesIO)

        buffer.seek(offset, io.SEEK_SET)

        self._contents = contents
        self._start_offset = offset
        self._end_offset = None
        self.buffer = buffer
        self._run = False

    @property
    def offset(self):
        return self.buffer.tell()

    def unpack(self, fmt):
        size = struct.calcsize(fmt)
        buf = self.buffer.read(size)
        return struct.unpack(fmt, buf)

    def read_string(self) -> str:
        start = self.offset
        return self.buffer.read(self._contents.find(b'\0', start) - start).decode('utf-8')

    def parse(self) -> dict:
        assert not self._run
        self._run = True

        buffer = self.buffer

        obj = {}
        # typ, name, value = None, None, None
        typ: int
        name: str
        value: dict | list

        # TODO: endianness is not specified
        while (typ := self.unpack('B')[0]) != Type.End:
            name = self.read_string()
            if typ == Type.NONE and not len(name) and not len(obj):
                name = self.read_string()

            match typ:
                case Type.NONE:
                    value = Parser(buffer, self.offset).parse()
                case Type.String:
                    value, = self.read_string()
                case Type.Int32 | Type.Color | Type.Pointer:
                    value, = self.unpack('<i')
                case Type.UInt64:
                    value, = self.unpack('<Q')
                case Type.Int64:
                    value, = self.unpack('<q')
                case Type.Float32:
                    value, = self.unpack('<f')
                case _:
                    raise TypeError((typ, self.offset, self._contents))

            obj[name] = convert_object(value)

        self._end_offset = self.offset
        return obj

    def bytes_length(self):
        if self._end_offset is None:
            self.parse()
        return self._end_offset - self._start_offset
