/*
   Copyright 2014-2015 Stanislav Ovsyannikov
   Copyright 2017 Anton Lechanka

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

	   Unless required by applicable law or agreed to in writing, software
	   distributed under the License is distributed on an "AS IS" BASIS,
	   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	   See the License for the specific language governing permissions and
	   limitations under the License.
*/

#include "decoder.h"
#include "log.h"

#include <limits.h>
#include <stdexcept>


namespace cbor
{

using std::to_string;

std::string to_string(majorType value)
{
    switch (value)
    {
        case majorType::unsignedInteger: return "unsignedInteger";
        case majorType::signedInteger: return "signedInteger";
        case majorType::byteString: return "byteString";
        case majorType::utf8String: return "utf8String";
        case majorType::array: return "array";
        case majorType::map: return "map";
        case majorType::tag: return "tag";
        case majorType::floatingPoint: return "floatingPoint";
        case majorType::simpleValue: return "simpleValue";
    }
    throw std::runtime_error("invalid major type");
}

decoder::decoder(input &in)
{
    _in = &in;
    _state = STATE_TYPE;
}

decoder::decoder(input &in, listener &listener)
{
    _in = &in;
    _listener = &listener;
    _state = STATE_TYPE;
}

decoder::~decoder()
{

}

void decoder::set_listener(listener & listener_instance)
{
    _listener = &listener_instance;
}

inline
int sizeFromAdditionalInfo(uint8_t in)
{
    const uint8_t additionalInfo = in & 0x1F;

    if (additionalInfo < 24) return 0;

    switch (additionalInfo)
    {
        case 24:
            return 1;
        case 25:
            return 2;
        case 26:
            return 4;
        case 27:
            return 8;
    }
    throw std::runtime_error("invalid additional info: " + to_string(additionalInfo));
}

void decoder::run()
{
    unsigned int temp;
    while (1)
    {
        const auto expected_len = _state == STATE_TYPE ? 1 : _currentLength;
        if (!_in->has_bytes(expected_len))
            break;

        switch (_state) {
            case STATE_TYPE: {
                unsigned char type = _in->get_byte();
                unsigned char majorType = type >> 5;
                auto minorType = (unsigned char) (type & 31);

                switch (majorType) {
                    case 0: // positive integer
                        if (minorType < 24)
                        {
                            _listener->on_integer(minorType);
                        } else if (minorType >= 24 and minorType <= 27)
                        {
                            _currentLength = sizeFromAdditionalInfo(minorType);
                            _state = STATE_PINT;
                        } else
                        {
                            _state = STATE_ERROR;
                            _listener->on_error("invalid integer type");
                        }
                        break;
                    case 1: // negative integer
                        if(minorType < 24) {
                            _listener->on_integer(-1 -minorType);
                        } else if(minorType == 24) { // 1 byte
                            _currentLength = 1;
                            _state = STATE_NINT;
                        } else if(minorType == 25) { // 2 byte
                            _currentLength = 2;
                            _state = STATE_NINT;
                        } else if(minorType == 26) { // 4 byte
                            _currentLength = 4;
                            _state = STATE_NINT;
                        } else if(minorType == 27) { // 8 byte
                            _currentLength = 8;
                            _state = STATE_NINT;
                        } else
                        {
                            _state = STATE_ERROR;
                            _listener->on_error("invalid integer type");
                        }
                        break;
                    case 2: // bytes
                        if (minorType < 24)
                        {
                            _state = STATE_BYTES_DATA;
                            _currentLength = minorType;
                        } else if (minorType >= 24 and minorType <= 27)
                        {
                            _currentLength = sizeFromAdditionalInfo(minorType);
                            _state = STATE_BYTES_SIZE;
                        } else
                        {
                            _state = STATE_ERROR;
                            _listener->on_error("invalid bytes type");
                        }
                        break;
                    case 3: // string
                        if (minorType < 24)
                        {
                            _state = STATE_STRING_DATA;
                            _currentLength = minorType;
                        } else if (minorType >= 24 and minorType <= 27)
                        {
                            _state = STATE_STRING_SIZE;
                            _currentLength = sizeFromAdditionalInfo(minorType);
                        } else
                        {
                            _state = STATE_ERROR;
                            _listener->on_error("invalid string type");
                        }
                        break;
                    case 4: // array
                        if (minorType < 24)
                        {
                            _listener->on_array(minorType);
                        } else if (minorType >= 24 and minorType <= 27)
                        {
                            _state = STATE_ARRAY;
                            _currentLength = sizeFromAdditionalInfo(minorType);
                        } else
                        {
                            _state = STATE_ERROR;
                            _listener->on_error("invalid array type");
                        }
                        break;
                    case 5: // map
                        if (minorType < 24)
                        {
                            _listener->on_map(minorType);
                        } else if (minorType >= 24 and minorType <= 27)
                        {
                            _state = STATE_MAP;
                            _currentLength = sizeFromAdditionalInfo(minorType);
                        } else
                        {
                            _state = STATE_ERROR;
                            _listener->on_error("invalid array type");
                        }
                        break;
                    case 6: // tag
                        if (minorType < 24)
                        {
                            _listener->on_tag(minorType);
                        } else if (minorType >= 24 and minorType <= 27)
                        {
                            _state = STATE_TAG;
                            _currentLength = sizeFromAdditionalInfo(minorType);
                        } else
                        {
                            _state = STATE_ERROR;
                            _listener->on_error("invalid tag type");
                        }
                        break;
                    case 7: // special
                        if (minorType < 20)
                        {
                            _listener->on_special(minorType);
                        } else if (minorType == 20)
                        {
                            _listener->on_bool(false);
                        } else if (minorType == 21)
                        {
                            _listener->on_bool(true);
                        } else if (minorType == 22)
                        {
                            _listener->on_null();
                        } else if (minorType == 23)
                        {
                            _listener->on_undefined();
                        } else if (minorType >= 24 and minorType <= 27)
                        {
                            _state = STATE_SPECIAL;
                            _currentLength = sizeFromAdditionalInfo(minorType);
                        } else
                        {
                            _state = STATE_ERROR;
                            _listener->on_error("invalid special type");
                        }
                        break;
                    default:
                        logger("unknown minor state in STATE_TYPE");
                }
                break;
            }
            case STATE_PINT: {
                switch (_currentLength)
                {
                    case 1:
                        _listener->on_integer(_in->get_byte());
                        break;
                    case 2:
                        _listener->on_integer(_in->get_short());
                        break;
                    case 4:
                        temp = _in->get_int();
                        if (temp <= INT_MAX)
                        {
                            _listener->on_integer(temp);
                        } else
                        {
                            _listener->on_extra_integer(temp, 1);
                        }
                        break;
                    case 8:
                        _listener->on_extra_integer(_in->get_long(), 1);
                        break;
                    default:
                        logger("unknown minor state in STATE_PINT");
                }
                _state = STATE_TYPE;
                break;
            }
            case STATE_NINT:
            {
                switch (_currentLength)
                {
                    case 1:
                        _listener->on_integer(-(int) _in->get_byte());
                        _state = STATE_TYPE;
                        break;
                    case 2:
                        _listener->on_integer(-(int) _in->get_short());
                        _state = STATE_TYPE;
                        break;
                    case 4:
                        temp = _in->get_int();
                        if (temp <= INT_MAX)
                        {
                            _listener->on_integer(-(int) temp);
                        } else if (temp == 2147483648u)
                        {
                            _listener->on_integer(INT_MIN);
                        } else
                        {
                            _listener->on_extra_integer(temp, -1);
                        }
                        _state = STATE_TYPE;
                        break;
                    case 8:
                        _listener->on_extra_integer(_in->get_long(), -1);
                        break;
                    default:
                        logger("unknown minor state in STATE_NINT");
                }
                break;
            }
            case STATE_BYTES_SIZE: {
                switch (_currentLength)
                {
                    case 1:
                        _currentLength = _in->get_byte();
                        _state = STATE_BYTES_DATA;
                        break;
                    case 2:
                        _currentLength = _in->get_short();
                        _state = STATE_BYTES_DATA;
                        break;
                    case 4:
                        _currentLength = _in->get_int();
                        _state = STATE_BYTES_DATA;
                        break;
                    case 8:
                        _state = STATE_ERROR;
                        _listener->on_error("extra long bytes");
                        break;
                    default:
                        logger("unknown minor state in STATE_BYTES_SIZE");
                }
                break;
            };
            case STATE_BYTES_DATA: {
                auto *data = new unsigned char[_currentLength];
                _in->get_bytes(data, _currentLength);
                _state = STATE_TYPE;
                _listener->on_bytes(data, _currentLength);
                break;
            }
            case STATE_STRING_SIZE: {
                switch (_currentLength)
                {
                    case 1:
                        _currentLength = _in->get_byte();
                        _state = STATE_STRING_DATA;
                        break;
                    case 2:
                        _currentLength = _in->get_short();
                        _state = STATE_STRING_DATA;
                        break;
                    case 4:
                        _currentLength = _in->get_int();
                        _state = STATE_STRING_DATA;
                        break;
                    case 8:
                        _state = STATE_ERROR;
                        _listener->on_error("extra long array");
                        break;
                    default:
                        logger("unknown minor state in STATE_STRING_SIZE");
                }
                break;
            }
            case STATE_STRING_DATA: {
                auto *data = new unsigned char[_currentLength];
                _in->get_bytes(data, _currentLength);
                _state = STATE_TYPE;
                std::string str((const char *) data, (size_t) _currentLength);
                _listener->on_string(str);
                break;
            }
            case STATE_ARRAY: {
                switch (_currentLength)
                {
                    case 1:
                        _listener->on_array(_in->get_byte());
                        _state = STATE_TYPE;
                        break;
                    case 2:
                        _listener->on_array(_currentLength = _in->get_short());
                        _state = STATE_TYPE;
                        break;
                    case 4:
                        _listener->on_array(_in->get_int());
                        _state = STATE_TYPE;
                        break;
                    case 8:
                        _state = STATE_ERROR;
                        _listener->on_error("extra long array");
                        break;
                    default:
                        logger("unknown minor state in STATE_ARRAY");
                }
                break;
            }
            case STATE_MAP: {
                switch (_currentLength)
                {
                    case 1:
                        _listener->on_map(_in->get_byte());
                        _state = STATE_TYPE;
                        break;
                    case 2:
                        _listener->on_map(_currentLength = _in->get_short());
                        _state = STATE_TYPE;
                        break;
                    case 4:
                        _listener->on_map(_in->get_int());
                        _state = STATE_TYPE;
                        break;
                    case 8:
                        _state = STATE_ERROR;
                        _listener->on_error("extra long map");
                        break;
                    default:
                        logger("unknown minor state in STATE_MAP");
                }
                break;
            }
            case STATE_TAG: {
                switch (_currentLength)
                {
                    case 1:
                        _listener->on_tag(_in->get_byte());
                        _state = STATE_TYPE;
                        break;
                    case 2:
                        _listener->on_tag(_in->get_short());
                        _state = STATE_TYPE;
                        break;
                    case 4:
                        _listener->on_tag(_in->get_int());
                        _state = STATE_TYPE;
                        break;
                    case 8:
                        _listener->on_extra_tag(_in->get_long());
                        _state = STATE_TYPE;
                        break;
                    default:
                        logger("unknown minor state in STATE_TAG");
                }
                break;
            }
            case STATE_SPECIAL: {
                switch (_currentLength)
                {
                    case 1:
                        _listener->on_special(_in->get_byte());
                        _state = STATE_TYPE;
                        break;
                    case 2:
                        _listener->on_special(_in->get_short());
                        _state = STATE_TYPE;
                        break;
                    case 4:
                        _listener->on_float(_in->get_float());
                        _state = STATE_TYPE;
                        break;
                    case 8:

                        _listener->on_double(_in->get_double());
                        _state = STATE_TYPE;
                        break;
                    default:
                        logger("unknown minor state in STATE_SPECIAL");
                }
                break;
            }
            case STATE_ERROR: {
                break;
            }
            default:
                logger("UNKNOWN STATE");
        }
    }
}

//* \brief returns next type without consuming it
type decoder::peekType() const
{
    uint8_t typeByte = _in->peek_byte();
    uint8_t majorTypeValue = typeByte >> 5;
    uint8_t minorType = typeByte & 0x1f;

    majorType typeEnum;
    size_t typeSize = sizeFromAdditionalInfo(minorType);

    switch (majorTypeValue)
    {
        case 0: // positive integer
            typeEnum = majorType::unsignedInteger;
            break;
        case 1: // negative integer
            typeEnum = majorType::signedInteger;
            break;
        case 2: // bytes
            typeEnum = majorType::byteString;
            // typeSize says, how many bytes to read for real size
            break;
        case 3: // string
            typeEnum = majorType::utf8String;
            // typeSize says, how many bytes to read for real size
            break;
        case 4: // array
            typeEnum = majorType::array;
            // typeSize says, how many bytes to read for real size
            break;
        case 5: // map
            typeEnum = majorType::map;
            // typeSize says, how many bytes to read for real size
            break;
        case 6: // tag
            typeEnum = majorType::tag;
            break;
        case 7: // special
            if (minorType > 19 and minorType < 24) typeEnum = majorType::simpleValue;
            if (minorType > 24 and minorType < 28) typeEnum = majorType::floatingPoint;
            break;
    }

    return cbor::type(typeEnum, typeSize, minorType);
}

size_t decoder::read_map()
{
    auto type = peekType();
    if (type.major() != majorType::map)
        throw std::runtime_error("wrong type" + to_string(type.major()) + " " + __FILE__ + ":" + to_string(__LINE__));
    _in->advance(1);

    return get_value<size_t>(type);
}

size_t decoder::read_array()
{
    auto type = peekType();
    if (type.major() != majorType::array)
        throw std::runtime_error("wrong type" + to_string(type.major()) + " " + __FILE__ + ":" + to_string(__LINE__));
    _in->advance(1);

    return get_value<size_t>(type);
}

void decoder::skip()
{
    auto type = peekType();
    _in->advance(1);

    switch (type.major())
    {
        case majorType::unsignedInteger:
        case majorType::signedInteger:
        case majorType::tag:
        case majorType::floatingPoint:
            _in->advance(type.size());
            break;
        case majorType::byteString:
        case majorType::utf8String:
        {
            size_t typeSize = get_value<size_t>(type);
            _in->advance(typeSize + type.size());
        }
            break;
        case majorType::array:
        {
            size_t typeSize = get_value<size_t>(type);

            while(typeSize--)
            {
                // skip sub element
                skip();
            }
        }
        break;
        case majorType::map:
        {
            size_t typeSize = get_value<size_t>(type);

            while(typeSize--)
            {
                skip(); // key
                skip(); // value
            }
        }
            break;

        case majorType::simpleValue:
            break;
    }
}

uint32_t decoder::read_uint()
{
    auto type = peekType();
    if (type.major() != majorType::unsignedInteger)
        throw std::runtime_error("wrong type " + to_string(type.major()) + " " + __FILE__ + ":" + to_string(__LINE__));
    _in->advance(1);

    return get_value<uint32_t>(type);
}

uint64_t decoder::read_ulong()
{
    auto type = peekType();
    if (type.major() != majorType::unsignedInteger)
        throw std::runtime_error("wrong type " + to_string(type.major()) + " " + __FILE__ + ":" + to_string(__LINE__));
    _in->advance(1);

    return get_value<uint64_t>(type);
}

int32_t decoder::read_int()
{
    auto type = peekType();
    if (type.major() != majorType::signedInteger && type.major() != majorType::unsignedInteger)
        throw std::runtime_error("wrong type " + to_string(type.major()) + " " + __FILE__ + ":" + to_string(__LINE__));
    _in->advance(1);

    int32_t value = get_value<int32_t>(type);

    if (type.major() == majorType::signedInteger)
        return -value;

    return value;
}

int64_t decoder::read_long()
{
    auto type = peekType();
    if (type.major() != majorType::signedInteger)
        throw std::runtime_error("wrong type " + to_string(type.major()) + " " + __FILE__ + ":" + to_string(__LINE__));
    _in->advance(1);

    int64_t value = get_value<uint64_t>(type);

    if (type.major() == majorType::signedInteger)
        return -value;
    return value;
}

float decoder::read_float()
{
    auto type = peekType();
    if (type.major() != majorType::floatingPoint and type.size() == 4)
        throw std::runtime_error("wrong type " + to_string(type.major()) + " " + __FILE__ + ":" + to_string(__LINE__));
    _in->advance(1);
    return _in->get_float();
}

double decoder::read_double()
{
    auto type = peekType();
    if (type.major() != majorType::floatingPoint and type.size() == 8)
        throw std::runtime_error("wrong type " + to_string(type.major()) + " " + __FILE__ + ":" + to_string(__LINE__));
    _in->advance(1);
    return _in->get_double();
}

std::string decoder::read_string()
{
    auto type = peekType();
    if (type.major() != majorType::byteString && type.major() != majorType::utf8String)
        throw std::runtime_error("wrong type " + to_string(type.major()) + " " + __FILE__ + ":" + to_string(__LINE__));
    _in->advance(1);

    size_t stringSize = get_value<size_t>(type);

    std::string tmpStr;
    tmpStr.resize(stringSize);

    _in->get_bytes((char*)tmpStr.data(), stringSize);

    return std::move(tmpStr);
}

bool decoder::read_bool()
{
    auto type = peekType();
    if (type.major() != majorType::simpleValue or (type.directValue() != 20 and type.directValue() != 21) )
        throw std::runtime_error("wrong type " + to_string(type.major()) + " " + __FILE__ + ":" + to_string(__LINE__));
    _in->advance(1);

    return type.directValue() == 21;
}

}
