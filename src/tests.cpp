/*
   Copyright 2014-2015 Stanislav Ovsyannikov

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

#include <stdio.h>
#include <iostream>
#include "cbor.h"

using std::cout;

int main() {
    cbor::output_dynamic output;

    { //encoding
        cbor::encoder encoder(output);
        encoder.write_array(11);
        {
            encoder.write_int(123);
            encoder.write_string("bar");
            encoder.write_int(321);
            encoder.write_int(321);
            encoder.write_string("foo");
            encoder.write_bool(true);
            encoder.write_bool(false);
            encoder.write_null();
            encoder.write_undefined();
            encoder.write_float(1.5);
            encoder.write_double(3.14152443);
        }
    }

    {
        cbor::input input(output.data(), output.size());
        cbor::decoder decoder(input);

        auto type = decoder.peekType();
        cout << "Type:\n";
        cout << " major: " << (int)type.major() << "\n";
        cout << " Size.: " << type.size() << "\n";

        size_t arrSize = decoder.read_array();

        cout << "Array with " << arrSize << " elements\n";

        for (auto i = 0u; i < arrSize; ++i)
        {
            type = decoder.peekType();
            cout << "[" << i << "]\n";
            cout << "Type:\n";
            cout << " major: " << (int)type.major() << "\n";
            cout << " Size.: " << type.size() << "\n";

            switch(type.major())
            {
                case cbor::majorType::unsignedInteger:
                    cout << "Int: " << decoder.read_uint() << "\n";
                    break;
                case cbor::majorType::signedInteger:
                    cout << "Int: " << decoder.read_int() << "\n";
                    break;
                case cbor::majorType::floatingPoint:
                    if (type.size() == 4)
                        cout << "Float: " << decoder.read_float() << "\n";
                    else
                        cout << "Double: " << decoder.read_double() << "\n";
                    break;
                default:
                    decoder.skip();
            }
        }
    }

    { // decoding
        cbor::input input(output.data(), output.size());
        cbor::listener_debug listener;
        cbor::decoder decoder(input, listener);
        decoder.run();
    }

    return 0;
}
