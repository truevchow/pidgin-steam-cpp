# https://github.com/gorgitko/valve-keyvalues-python/blob/master/valve_keyvalues_python/keyvalues.py
# node-kvparser/lib/KvParser.js

__author__ = "Jiri Novotny"
__version__ = "1.0.0"

import collections
import re
import sys
from typing import io

import typing.io


class KeyValues(dict):
    """
    Class for manipulation with Valve KeyValue (KV) files (VDF format).
    Parses the KV file to object with dict interface.
    Allows to write objects with dict interface to KV files.
    """

    __re = re
    __sys = sys
    __OrderedDict = collections.OrderedDict
    __regexs = {
        "key": __re.compile(r"""(['"])(?P<key>((?!\1).)*)\1(?!.)""", __re.I),
        "key_value": __re.compile(r"""(['"])(?P<key>((?!\1).)*)\1(\s+|)['"](?P<value>((?!\1).)*)\1""", __re.I)
    }

    def __init__(self, stream: typing.io.TextIO | str | None, mapper=None, mapper_type=__OrderedDict, key_modifier=None,
                 key_sorter=None):
        """
        :param mapper: initialize with own dict-like mapper
        :param mapper_type: which mapper will be used for storing KV. It must have the dict interface, i.e. allow to do the 'mapper[key] = value action'.
                default: 'collections.OrderedDict'
                For example you can use the 'dict' type.
        :param key_modifier: function for modifying the keys, e.g. the function 'string.lower' will make all the keys lower
        :param key_sorter: function for sorting the keys when dumping/writing/str, e.g. using the function 'sorted' will show KV keys in alphabetical order
        """

        super().__init__()
        self.__sys.setrecursionlimit(100000)
        self.mapper_type = type(mapper) if mapper else mapper_type
        self.key_modifier = key_modifier
        self.key_sorter = key_sorter

        if not mapper and stream is None:
            self.__mapper = mapper_type()
            return

        if mapper:
            self.__mapper = mapper
            return

        if isinstance(stream, str):
            stream = io.TextIO(stream)
        self.parse(stream)

    def __setitem__(self, key, item):
        self.__mapper[key] = item

    def __getitem__(self, key):
        return self.__mapper[key]

    def __repr__(self):
        # return repr(self.__mapper)
        return self.dump(self.__mapper)

    def __len__(self):
        return len(self.__mapper)

    def __delitem__(self, key):
        del self.__mapper[key]

    def clear(self):
        return self.__mapper.clear()

    def copy(self):
        """
        :return: mapper of KeyValues
        """
        return self.__mapper.copy()

    def has_key(self, k):
        return k in self.__mapper

    def pop(self, k, d=None):
        return self.__mapper.pop(k, d)

    def update(self, *args, **kwargs):
        return self.__mapper.update(*args, **kwargs)

    def keys(self):
        return self.__mapper.keys()

    def values(self):
        return self.__mapper.values()

    def items(self):
        return self.__mapper.items()

    def __eq__(self, other: dict):
        return self.__mapper == dict

    def __ne__(self, other):
        return self.__mapper == dict

    def __contains__(self, item):
        return item in self.__mapper

    def __iter__(self):
        return iter(self.__mapper)

    def __str__(self):
        return self.dumps()

    def __key_modifier(self, key, key_modifier):
        """
        Modifies the key string using the 'key_modifier' function.

        :param key:
        :param key_modifier:
        :return:
        """

        key_modifier = key_modifier or self.key_modifier

        if key_modifier:
            return key_modifier(key)
        else:
            return key

    def __parse(self, lines, mapper_type, i=0, key_modifier=None):
        """
        Recursively maps the KeyValues from list of file lines.

        :param lines:
        :param mapper_type:
        :param i:
        :param key_modifier:
        :return:
        """

        key = False
        _mapper = mapper_type()

        try:
            while i < len(lines):
                if lines[i].startswith("{"):
                    if not key:
                        raise Exception("'{{' found without key at line {}".format(i + 1))
                    _mapper[key], i = self.__parse(lines, i=i + 1, mapper_type=mapper_type, key_modifier=key_modifier)
                    continue
                elif lines[i].startswith("}"):
                    return _mapper, i + 1
                elif self.__re.match(self.__regexs["key"], lines[i]):
                    key = self.__key_modifier(self.__re.search(self.__regexs["key"], lines[i]).group("key"),
                                              key_modifier)
                    i += 1
                    continue
                elif self.__re.match(self.__regexs["key_value"], lines[i]):
                    groups = self.__re.search(self.__regexs["key_value"], lines[i])
                    _mapper[self.__key_modifier(groups.group("key"), key_modifier)] = groups.group("value")
                    i += 1
                elif self.__re.match(self.__regexs["key_value"], lines[i] + lines[i + 1]):
                    groups = self.__re.search(self.__regexs["key_value"], lines[i] + " " + lines[i + 1])
                    _mapper[self.__key_modifier(groups.group("key"), key_modifier)] = groups.group("value")
                    i += 1
                else:
                    i += 1
        except IndexError:
            pass

        return _mapper

    @classmethod
    def open(cls, filename, encoding="utf-8", **kwargs):
        with open(filename, mode="r", encoding=encoding) as f:
            return cls(f, **kwargs)

    def parse(self, stream: typing.io.TextIO, mapper_type=__OrderedDict, key_modifier=None):
        """
        Parses the KV file so this instance can be accessed by dict interface.

        :param stream:
        :param mapper_type: which mapper will be used for storing KV. It must have the dict interface, i.e. allow to do the 'mapper[key] = value action'.
                default: 'collections.OrderedDict'
                For example you can use the 'dict' type.
                This will override the instance's 'mapper_type' if specified during instantiation.
        :param key_modifier: function for modifying the keys, e.g. the function 'string.lower' will make all the keys lower.
                This will override the instance's 'key_modifier' if specified during instantiation.
        """
        self.__mapper = self.__parse([line.strip() for line in stream.readlines()],
                                     mapper_type=mapper_type or self.mapper_type,
                                     key_modifier=key_modifier or self.key_modifier)

    @staticmethod
    def __tab(string, level, quotes=False):
        if quotes:
            return '{}"{}"'.format(level * "\t", string)
        else:
            return '{}{}'.format(level * "\t", string)

    def __dumps(self, mapper, key_sorter=None, level=0):
        string = ""

        if key_sorter:
            keys = key_sorter(mapper.keys())
        else:
            keys = mapper.keys()

        for key in keys:
            string += self.__tab(key, level, quotes=True)
            if type(mapper[key]) == str:
                string += '\t "{}"\n'.format(mapper[key])
            else:
                string += "\n" + self.__tab("{\n", level)
                string += self.__dumps(mapper[key], key_sorter=key_sorter, level=level + 1)
                string += self.__tab("}\n", level)

        return string

    def dumps(self, mapper=None, key_sorter=None):
        """
        Dumps the KeyValues mapper to string.

        :param mapper: you can dump your own object with dict interface
        :param key_sorter: function for sorting the keys when dumping/writing/str, e.g. using the function 'sorted' will show KV in alphabetical order.
                This will override the instance's 'key_sorter' if specified during instantiation.
        :return: string
        """

        return self.__dumps(mapper=mapper or self.__mapper, key_sorter=key_sorter or self.key_sorter)

    def dump(self, filename, encoding="utf-8", mapper=None, key_sorter=None):
        """
        Writes the KeyValues to file.

        :param filename: output KV file name
        :param encoding: output KV file encoding. Default: 'utf-8'
        :param mapper: you can write your own object with dict interface
        :param key_sorter: key_sorter: function for sorting the keys when dumping/writing/str, e.g. using the function 'sorted' will show KV in alphabetical order.
                This will override the instance's 'key_sorter' if specified during instantiation.
        """

        with open(filename, mode="w", encoding=encoding) as f:
            f.write(self.dumps(mapper=mapper or self.__mapper, key_sorter=key_sorter or self.key_sorter))
