#include "ApplicationVolumeRender.h"

#undef main
int main(int argc, char* argv[]) {

	try {
		auto appDesc = ApplicationDesc{};
		appDesc.Width = 1280;
		appDesc.Height = 720;
		appDesc.Tittle = "Application VolumeRender <DX11>";
		appDesc.IsFullScreen = false;
		
		
		auto pAppication = std::make_unique<ApplicationVolumeRender>(appDesc);
		pAppication->Run();
	} catch (std::exception const& e) {
		std::cout << e.what() << std::endl;
		assert(0);
	}
}



