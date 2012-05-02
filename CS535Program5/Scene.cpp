#include <fstream>
#include <iostream>
#include <algorithm>
#include <iomanip>

using namespace std;

#include "Scene.h"
#include "SceneLoader.h"

Scene::Scene():
	width(0),
	height(0),
	maxlevel(0),
	antialiasing(false),
	pixels(NULL)
{}

Scene::Scene(int width, int height, int maxlevel, bool antialiasing):
	width(width),
	height(height),
	maxlevel(maxlevel),
	antialiasing(antialiasing),
	pixels(NULL)
{	
	pixels = new float[height * width * 3];
}

Result Scene::load(string filename) {
	destroy();
	return SceneLoader().load(filename, *this);
}

void Scene::status() {
	int FIELD = 20;
	string edge = "| ";
	cout << left;
	cout << setw(FIELD) << "Scene content:" << endl;
	cout << setw(FIELD) << " Shapes" << edge << shapes.size() << endl;
	cout << setw(FIELD) << " Point Lights" << edge << pointLights.size() << endl;
	cout << setw(FIELD) << "Scene settings:" << endl;
	cout << setw(FIELD) << " Height" << edge << height << endl;
	cout << setw(FIELD) << " Width" << edge << width << endl;
	cout << setw(FIELD) << " Max Recursion" << edge << maxlevel << endl;
	cout << setw(FIELD) << " Antialiasing" << edge << (antialiasing ? "ON" : "OFF") << endl;
}

Scene::~Scene() {
	delete pixels;
	pixels = NULL;
	destroy();
}

void Scene::destroy() {
	for(size_t i = 0; i < shapes.size(); ++i) {
		delete shapes[i];
	}
	shapes.clear();

	for(size_t i = 0; i < pointLights.size(); ++i) {
		delete pointLights[i];
	}
	pointLights.clear();

	for(size_t i = 0; i < areaLights.size(); ++i) {
		delete areaLights[i];
	}
	areaLights.clear();
}

void Scene::add(Shape *shape) {
	shapes.push_back(shape);
}

void Scene::add(PointLight *light) {
	pointLights.push_back(light);
}

void Scene::add(AreaLight *light) {
	areaLights.push_back(light);
}

void Scene::setAmbient(glm::vec3 &ambient) {
	this->ambient = ambient;
}

void Scene::raytrace() {
	glm::vec3 eye(0.0, 0.0, 1.5);
	int step = startProgress();

	float inc = 1.0f, adj = 1.0f;
	if(antialiasing) {
		inc = 0.5f;
		adj = inc * inc;
	}

	for(int i = 0; i < height; ++i) {
		for(int j = 0; j < width; ++j) {
			glm::vec3 color(0);
			for(float isample = 0.0f; isample < 1.0f; isample += inc) {
				float y = (i + isample) * (1.0 / height) * 2 - 1;
				for(float jsample = 0.0f; jsample < 1.0f; jsample += inc) {
					float x = (j + jsample) * (1.0 / width) * 2 - 1;
					Ray ray(eye, glm::normalize(glm::vec3(x, y, 0.0) - eye)); 
					color += trace(ray, 0);
				}
			}
			color *= adj;
			pixels[AT_R(i, j)] = color.r;
			pixels[AT_G(i, j)] = color.g;
			pixels[AT_B(i, j)] = color.b;
		}
		updateProgress(i, step);
	}
	endProgress();
}

void Scene::draw() {
	glRasterPos2i(0, 0);
	glDrawPixels(width, height, GL_RGB, GL_FLOAT, pixels);
}

glm::vec3 Scene::trace(Ray &ray, int level) {
	if(level >= maxlevel) {
		return glm::vec3(0);
	}

	Shape *shape = NULL;
	float t, closest = 10000000.0f;
	for(size_t i = 0; i < shapes.size(); ++i) {
		if(shapes[i]->intersect(ray, t)) {
			if(t < closest) {
				closest = t;
				shape = shapes[i];
			}
		}
	}

	if(shape == NULL) {
		return glm::vec3(0);
	} else {
		Material m;
		glm::vec3 intersection = ray.origin + ray.direction * closest;
		shape->shading(intersection, m);
		glm::vec3 color = ambient * m.diffuse;
		for(size_t i = 0; i < pointLights.size(); ++i) {
			glm::vec3 toLight = pointLights[i]->position - intersection;
			float distance = glm::length(toLight);
			glm::vec3 lightDir = toLight / distance;
			Ray shadow(intersection, lightDir);
			bool occluded = false;
			for(size_t j = 0; j < shapes.size(); ++j) {
				float t;
				if(shapes[j] != shape && shapes[j]->intersect(shadow, t) && t < distance) {
					occluded = true;
					break;
				} 
			}
			if(!occluded) {
				glm::vec3 diffuse = m.diffuse * max(0.0f, glm::dot(m.normal, lightDir));
				glm::vec3 specular = m.specular * pow(max(0.0f, glm::dot(-ray.direction, glm::reflect(-lightDir, m.normal))), m.shiny);
				color += pointLights[i]->color * (diffuse + specular);
			}
		}
		if(level < maxlevel) {
			if(m.reflection > 0) {
				//color += m.reflection * trace(reflect(shape, intersection, ray.direction, m.normal), level + 1);
				color = color * (1.0f - m.reflection) + m.reflection * trace(reflect(shape, intersection, ray.direction, m.normal), level + 1);
			}
		}
		return glm::clamp(color, 0.0f, 1.0f);
	}
	return glm::vec3(0);
}

Ray Scene::reflect(Shape *shape, glm::vec3 &intersection, glm::vec3 &direction, glm::vec3 &normal) {
	Ray newRay(intersection, glm::normalize(glm::reflect(direction, normal)));
	nudge(shape, newRay);
	return newRay;
}

void Scene::nudge(Shape *shape, Ray &ray) {
	float t;
	if(shape->intersect(ray, t)) {
		ray.origin += ray.direction * (t + 0.001f);
	}
}

