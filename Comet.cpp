#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "Comet.h"
#include "App/MainWindow.h"

int main()
{
	MainWindow mainWindow((std::string("Comet v")+COMET_VERSION+"-"+COMET_STAGE).c_str());
	mainWindow.Run();
	return 0;
}
