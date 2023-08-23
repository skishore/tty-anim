def Settings(**kwargs):
    flags = "-x c++ -lncurses -Iabseil-cpp -std=c++1z -Wall -Werror -resource-dir=/Library/Developer/CommandLineTools/usr/lib/clang/14.0.3"
    return {"flags": flags.split()}
