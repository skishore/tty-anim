def Settings(**kwargs):
    flags = "-x c++ -Iabseil-cpp -std=c++1z -Wall -Werror -Wno-unused -resource-dir=/Library/Developer/CommandLineTools/usr/lib/clang/14.0.3"
    return {"flags": flags.split()}
