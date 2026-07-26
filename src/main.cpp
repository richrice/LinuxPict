#include "linuxpict/app.hpp"
int main(int argc, char* argv[]) {
    return linuxpict::LinuxPictApplication::create()->run(argc, argv);
}
