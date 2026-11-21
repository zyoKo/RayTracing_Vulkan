#include "core/application.h"

int main(int Argc, char** Argv)
{
    rtrt::Application App;
    if (!App.Init(Argc, Argv))
    {
        return 1;
    }
    return App.Run();
}
