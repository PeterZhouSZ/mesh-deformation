#include "pmp/visualization/MeshViewer.h"
#include "pmp/algorithms/SurfaceSmoothing.h"
#include "VertexSelectionViewer.hpp"

int main()
{
	using namespace pmp;

	const std::string meshFile = "../models/elephant.off";
	VertexSelectionViewer window("hello world! Test", 1366, 768);
	window.load_mesh(meshFile.c_str());
	window.run();
	return 0;
}