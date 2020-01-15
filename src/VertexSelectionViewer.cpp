#include "VertexSelectionViewer.hpp"
#include "algorithms/deformation.hpp"
#include <pmp/algorithms/SurfaceNormals.h>

using namespace pmp;

VertexSelectionViewer::VertexSelectionViewer(const char* title, int width, int height, bool showgui)
	: TrackballViewer(title, width, height, showgui), pickPosition_(0, 0, 0), pickVertex_(0), isVertexTranslationActive_(false),
	brushSize_(0.05), isVertexTranslationMouseActive_(false)
{
	clear_draw_modes();
	add_draw_mode("Smooth Shading");
	set_draw_mode("Smooth Shading");
}

VertexSelectionViewer::~VertexSelectionViewer()
{
}

void VertexSelectionViewer::do_processing()
{
}

void VertexSelectionViewer::draw(const std::string& draw_mode)
{
	update_mesh();
	mesh_.draw(projection_matrix_, modelview_matrix_, "Smooth Shading");
}

void VertexSelectionViewer::keyboard(int key, int scancode, int action, int mods)
{
	if (action != GLFW_PRESS && action != GLFW_REPEAT)
		return;

	switch (key)
	{
	case GLFW_KEY_BACKSPACE: // reload model
	{
		load_mesh(filename_.c_str());
		deformationSpace_.reset(nullptr);
		break;
	}
	case GLFW_KEY_W: //support region
	{
		TrackballViewer::pick(pickPosition_);
		double x = 0;
		double y = 0;
		cursor_pos(x, y);
		auto vVector = pick_vertex(x, y, brushSize_);

		if (!vVector.empty())
		{
			auto vProp = mesh_.get_vertex_property<Color>("v:col");
			for (auto v : vVector)
				vProp[v] = Color(0, 0, 1);
			update_mesh();
		}
		break;
	}
	case GLFW_KEY_Q: //handle region
	{
		TrackballViewer::pick(pickPosition_);
		double x = 0;
		double y = 0;
		cursor_pos(x, y);
		auto vVector = pick_vertex(x, y, brushSize_);

		if (!vVector.empty())
		{
			auto vProp = mesh_.get_vertex_property<Color>("v:col");
			for (auto v : vVector)
				vProp[v] = Color(0, 1, 0);
			update_mesh();
		}
		break;
	}
	case GLFW_KEY_R:
	{
		TrackballViewer::pick(pickPosition_);
		double x = 0;
		double y = 0;
		cursor_pos(x, y);
		auto v = pick_vertex(x, y);

		if (v.is_valid())
		{
			auto vProp = mesh_.get_vertex_property<Color>("v:col");
			vProp[v] = Color(1, 0, 0);
			update_mesh();
		}
		break;
	}
	case GLFW_KEY_I:
	{
		deformationSpace_ = std::make_unique<algorithm::Deformation>(mesh_);
		auto colors = mesh_.get_vertex_property<Color>("v:col");
		std::vector<Vertex> supportVertices;
		std::vector<Vertex> handleVertices;
		pmp::Normal normal(0, 0, 0);
		int normalIndex = 0;

		for (Vertex v : mesh_.vertices())
		{
			if (colors[v] == Color(0, 1, 0))
			{
				handleVertices.push_back(v);
				normal += SurfaceNormals::compute_vertex_normal(mesh_, v);
				normalIndex++;
			}
			else if (colors[v] == Color(0, 0, 1))
			{
				supportVertices.push_back(v);
			}
		}

		normal.normalize();
		translationNormal_ = normal;

		deformationSpace_->set_regions(supportVertices, handleVertices);
		break;
	}
	case GLFW_KEY_SPACE:
	{
		isVertexTranslationActive_ = !isVertexTranslationActive_;
		break;
	}
	case GLFW_KEY_T:
	{
		static algorithm::Deformation deformation(mesh_);
		static bool init = true;

		if (init)
		{
			init = false;
			auto points = mesh_.get_vertex_property<Point>("v:point");
			auto colors = mesh_.get_vertex_property<Color>("v:col");
			std::vector<Vertex> supportVertices;
			std::vector<Vertex> handleVertices;
			for (Vertex v : mesh_.vertices())
			{
				if (points[v][2] > 80.0)
				{
					colors[v] = Color(0.0, 1.0, 0.0);
					handleVertices.push_back(v);
				}
				else if (points[v][2] > 0.0)
				{
					colors[v] = Color(0.0, 0.0, 1.0);
					supportVertices.push_back(v);
				}
			}
			deformation.set_regions(supportVertices, handleVertices);
		}
		else deformation.translate(Normal(0.0, 0.0, 1.0));

		update_mesh();
	}
	default:
	{
		TrackballViewer::keyboard(key, scancode, action, mods);
		break;
	}
	}
}

bool VertexSelectionViewer::load_mesh(const char* filename)
{
	// load mesh
	if (mesh_.read(filename))
	{
		// update scene center and bounds
		BoundingBox bb = mesh_.bounds();
		set_scene((vec3)bb.center(), 0.5 * bb.size());

		auto vProp = mesh_.add_vertex_property<Color>("v:col");
		for (auto v : mesh_.vertices())
			vProp[v] = Color(1, 0, 0);

		// compute face & vertex normals, update face indices
		update_mesh();

		// set draw mode
		if (mesh_.n_faces())
		{
			set_draw_mode("Solid Smooth");
		}
		else if (mesh_.n_vertices())
		{
			set_draw_mode("Points");
		}

		// print mesh statistic
		std::cout << "Load " << filename << ": " << mesh_.n_vertices()
			<< " vertices, " << mesh_.n_faces() << " faces\n";

		filename_ = filename;
		return true;
	}

	std::cerr << "Failed to read mesh from " << filename << " !" << std::endl;
	return false;
}

void VertexSelectionViewer::motion(double xpos, double ypos)
{
	if (isVertexTranslationMouseActive_)
	{
		vec2 currMousePos = vec2(xpos, ypos);
		vec2 mouseMotion = currMousePos - vec2(last_point_2d_[0], last_point_2d_[1]);
		float mouseMotionNorm = pmp::norm(mouseMotion);
		if (ypos > 0 && mouseMotionNorm > 0)
		{
			vec4 t = projection_matrix_ * modelview_matrix_ * vec4(translationNormal_, 1.0f);
			vec2 tVec2 = vec2(-t[0], -t[1]);

			tVec2.normalize();
			mouseMotion.normalize();

			float scalar = pmp::dot(mouseMotion, tVec2);
			std::cout << scalar << std::endl;
			deformationSpace_->translate(this->translationNormal_ * scalar * 0.001f * mouseMotionNorm);
			last_point_2d_ = ivec2(xpos, ypos);
		}
	}
	else
	{
		TrackballViewer::motion(xpos, ypos);
	}
}

void VertexSelectionViewer::mouse(int button, int action, int mods)
{
	if (action == GLFW_PRESS && isVertexTranslationActive_)
		this->isVertexTranslationMouseActive_ = true;
	else
	{
		this->isVertexTranslationMouseActive_ = false;
		TrackballViewer::mouse(button, action, mods);
	}
}

Vertex VertexSelectionViewer::pick_vertex(int x, int y)
{
	Vertex vmin;

	vec3 p;
	Scalar d, dmin(FLT_MAX);

	if (TrackballViewer::pick(x, y, p))
	{
		Point picked_position(p);
		for (auto v : mesh_.vertices())
		{
			d = distance(mesh_.position(v), picked_position);
			if (d < dmin)
			{
				dmin = d;
				vmin = v;
			}
		}
	}
	return vmin;
}

std::vector<Vertex> VertexSelectionViewer::pick_vertex(int x, int y, float radius)
{
	std::vector<Vertex> vVector;

	vec3 p;
	Scalar d;

	if (TrackballViewer::pick(x, y, p))
	{
		Point picked_position(p);
		for (auto v : mesh_.vertices())
		{
			d = distance(mesh_.position(v), picked_position);
			if (d < radius)
			{
				vVector.push_back(v);
			}
		}
	}
	return vVector;
}

void VertexSelectionViewer::process_imgui()
{
	if (ImGui::CollapsingHeader("MousePosition", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// output mesh statistics
		ImGui::BulletText("%f X:", pickPosition_[0]);
		ImGui::BulletText("%f Y:", pickPosition_[1]);
		ImGui::BulletText("%f Z:", pickPosition_[2]);
		const BoundingBox bb = mesh_.bounds();
		ImGui::SliderFloat("%f BrushSize:", &brushSize_, 0.01, bb.size());
	}
}

void VertexSelectionViewer::update_mesh()
{
	// update scene center and radius, but don't update camera view
	BoundingBox bb = mesh_.bounds();
	center_ = (vec3)bb.center();
	radius_ = 0.5f * bb.size();

	// re-compute face and vertex normals
	mesh_.update_opengl_buffers();
}
