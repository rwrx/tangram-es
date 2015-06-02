#include "sceneLoader.h"

#include <vector>
#include "platform.h"
#include "scene.h"
#include "tileManager.h"
#include "view.h"
#include "lights.h"
#include "geoJsonSource.h"
#include "mvtSource.h"
#include "polygonStyle.h"
#include "polylineStyle.h"
#include "textStyle.h"
#include "debugStyle.h"
#include "debugTextStyle.h"
#include "filters.h"

#include "yaml-cpp/yaml.h"

using namespace YAML;
using namespace Tangram;

void SceneLoader::loadScene(const std::string& _file, Scene& _scene, TileManager& _tileManager, View& _view) {

    std::string configString = stringFromResource(_file.c_str());

    Node config = YAML::Load(configString);

    loadSources(config["sources"], _tileManager);
    loadStyles(config["styles"], _scene);
    loadLayers(config["layers"], _scene, _tileManager);
    loadCameras(config["cameras"], _view);
    loadLights(config["lights"], _scene);

    for (auto& style : _scene.getStyles()) {
        style->build(_scene.getLights());
    }

}

glm::vec4 parseVec4(const Node& node) {
    glm::vec4 vec;
    int i = 0;
    for (auto it = node.begin(); it != node.end(); ++it) {
        if (i < 4) {
            vec[i++] = it->as<float>();
        } else {
            break;
        }
    }
    return vec;
}

glm::vec3 parseVec3(const Node& node) {
    glm::vec3 vec;
    int i = 0;
    for (auto it = node.begin(); it != node.end(); ++it) {
        if (i < 3) {
            vec[i++] = it->as<float>();
        } else {
            break;
        }
    }
    return vec;
}

std::shared_ptr<Material> SceneLoader::loadMaterial(YAML::Node matNode) {

    auto mat = std::make_shared<Material>();

    Node diffuse = matNode["diffuse"];
    if (diffuse) {
        if (diffuse.IsMap()) {
            // Parse texture parameters
        } else {
            // Just set a color
            mat->setDiffuse(parseVec4(diffuse));
        }
    }

    Node ambient = matNode["ambient"];
    if (ambient) {
        if (ambient.IsMap()) {
            // Parse texture parameters
        } else {
            // Just set a color
            mat->setAmbient(parseVec4(ambient));
        }
    }

    Node specular = matNode["specular"];
    if (specular) {
        if (specular.IsMap()) {
            // Parse texture parameters
        } else {
            // Just set a color
            mat->setSpecular(parseVec4(specular));
        }
    }

    Node shininess = matNode["shininess"];
    if (shininess) { mat->setShininess(shininess.as<float>()); }

    Node normal = matNode["normal"];
    if (normal) {
        Node texture = normal["texture"];
        Node mapping = normal["mapping"];
        if (texture && mapping) {

            std::string texstring = texture.as<std::string>();
            std::string mapstring = mapping.as<std::string>();

            MappingType maptype = MappingType::uv;
            if (mapstring == "uv") { maptype = MappingType::uv; }
            else if (mapstring == "planar") { maptype = MappingType::planar; }
            else if (mapstring == "triplanar") { maptype = MappingType::triplanar; }
            else { logMsg("WARNING: invalid mapping for texture \"%s\"\n", texstring.c_str()); }

            mat->setNormal(texstring, maptype);
        }
    }

    return mat;

}

void SceneLoader::loadStyles(YAML::Node styles, Scene& scene) {

    // Instantiate built-in styles
    scene.getStyles().emplace_back(new PolygonStyle("polygons"));
    scene.getStyles().emplace_back(new PolylineStyle("lines"));
    scene.getStyles().emplace_back(new TextStyle("FiraSans", "text", 15.0f, 0xF7F0E1, true, true));
    scene.getStyles().emplace_back(new DebugTextStyle("FiraSans", "debugtext", 30.0f, 0xDC3522, true));
    scene.getStyles().emplace_back(new DebugStyle("debug"));

    if (!styles) {
        return;
    }

    for (auto styleIt : styles) {

        Style* style = nullptr;

        std::string styleName = styleIt.first.as<std::string>();
        Node styleNode = styleIt.second;

        Node baseNode = styleNode["base"];
        if (baseNode) {
            std::string baseString = baseNode.as<std::string>();
            if (baseString == "lines") { style = new PolylineStyle(styleName); }
            else if (baseString == "text") { style = new TextStyle("FiraSans", styleName, 15.0f, 0xF7F0E1, true, true); }
            else if (baseString == "sprites") { /* TODO */ }
        }

        if (style == nullptr) { style = new PolygonStyle(styleName); }

        Node animatedNode = styleNode["animated"];
        if (animatedNode) { /* TODO */ }

        Node blendNode = styleNode["blend"];
        if (blendNode) {

            std::string blend = blendNode.as<std::string>();
            if (blend == "add") { /* TODO */ }
            else if (blend == "multiply") { /* TODO */ }

        }

        Node texcoordsNode = styleNode["texcoords"];
        if (texcoordsNode) {

            if (texcoordsNode.as<bool>()) { /* TODO */ }
            else { /* TODO */ }

        }

        Node shadersNode = styleNode["shaders"];
        if (shadersNode) { /* TODO */ }

        Node materialNode = styleNode["material"];
        if (materialNode) {
            std::shared_ptr<Material> material = loadMaterial(materialNode);
            if (material) {
                style->setMaterial(material);
            }
        }
        
        Node lightingNode = styleNode["lighting"];
        if (lightingNode) {
            if (lightingNode.as<std::string>() == "fragment") { style->setLightingType(LightingType::fragment); }
            else if (lightingNode.as<std::string>() == "vertex") { style->setLightingType(LightingType::vertex); }
            else { style->setLightingType(LightingType::vertex); }
        }

        Node urlNode = styleNode["url"];
        if (urlNode) { /* TODO */ }

        Node namedStyleNode = styleNode["style"];
        if (namedStyleNode) { /* TODO */ }
        
        scene.addStyle(std::unique_ptr<Style>(style));

    }

}

void SceneLoader::loadSources(Node sources, TileManager& tileManager) {

    if(!sources) {
        logMsg("Warning: No source defined in the yaml scene configuration.\n");
        return;
    }

    for (auto it = sources.begin(); it != sources.end(); ++it) {

        const Node source = it->second;
        std::string name = it->first.as<std::string>();
        std::string type = source["type"].as<std::string>();
        std::string url = source["url"].as<std::string>();

        std::unique_ptr<DataSource> sourcePtr;

        if (type == "GeoJSONTiles") {
            sourcePtr = std::unique_ptr<DataSource>(new GeoJsonSource(name, url));
        } else if (type == "TopoJSONTiles") {
            // TODO
        } else if (type == "MVT") {
            sourcePtr = std::unique_ptr<DataSource>(new MVTSource(name, url));
        }

        if (sourcePtr) {
            tileManager.addDataSource(std::move(sourcePtr));
        }
    }

}

void SceneLoader::loadLights(Node lights, Scene& scene) {

    if (!lights) {

        // Add an ambient light if nothing else is specified
        std::unique_ptr<AmbientLight> amb(new AmbientLight("defaultLight"));
        amb->setAmbientColor({ .5f, .5f, .5f, 1.f });
        scene.addLight(std::move(amb));

        return;
    }

    for (auto it = lights.begin(); it != lights.end(); ++it) {

        const Node light = it->second;
        const std::string name = it->first.Scalar();
        const std::string type = light["type"].as<std::string>();

        std::unique_ptr<Light> lightPtr;

        if (type == "ambient") {

            lightPtr = std::unique_ptr<Light>(new AmbientLight(name));

        } else if (type == "directional") {

            DirectionalLight* dLightPtr = new DirectionalLight(name);
            Node direction = light["direction"];
            if (direction) {
                dLightPtr->setDirection(parseVec3(direction));
            }
            lightPtr = std::unique_ptr<Light>(dLightPtr);

        } else if (type == "point") {

            PointLight* pLightPtr = new PointLight(name);
            Node position = light["position"];
            if (position) {
                pLightPtr->setPosition(parseVec3(position));
            }
            Node radius = light["radius"];
            if (radius) {
                if (radius.size() > 1) {
                    pLightPtr->setRadius(radius[0].as<float>(), radius[1].as<float>());
                } else {
                    pLightPtr->setRadius(radius.as<float>());
                }
            }
            Node att = light["attenuation"];
            if (att) {
                pLightPtr->setAttenuation(att.as<float>());
            }
            lightPtr = std::unique_ptr<Light>(pLightPtr);

        } else if (type == "spotlight") {

            SpotLight* sLightPtr = new SpotLight(name);
            Node position = light["position"];
            if (position) {
                sLightPtr->setPosition(parseVec3(position));
            }
            Node direction = light["direction"];
            if (direction) {
                sLightPtr->setDirection(parseVec3(direction));
            }
            Node radius = light["radius"];
            if (radius) {
                if (radius.size() > 1) {
                    sLightPtr->setRadius(radius[0].as<float>(), radius[1].as<float>());
                } else {
                    sLightPtr->setRadius(radius.as<float>());
                }
            }
            Node angle = light["angle"];
            if (angle) {
                sLightPtr->setCutoffAngle(angle.as<float>());
            }
            Node exponent = light["exponent"];
            if (exponent) {
                sLightPtr->setCutoffExponent(exponent.as<float>());
            }

            lightPtr = std::unique_ptr<Light>(sLightPtr);

        }

        Node origin = light["origin"];
        if (origin) {
            const std::string originStr = origin.as<std::string>();
            if (originStr == "world") {
                lightPtr->setOrigin(LightOrigin::WORLD);
            } else if (originStr == "camera") {
                lightPtr->setOrigin(LightOrigin::CAMERA);
            } else if (originStr == "ground") {
                lightPtr->setOrigin(LightOrigin::GROUND);
            }
        }

        Node ambient = light["ambient"];
        if (ambient) {
            lightPtr->setAmbientColor(parseVec4(ambient));
        }

        Node diffuse = light["diffuse"];
        if (diffuse) {
            lightPtr->setDiffuseColor(parseVec4(diffuse));
        }

        Node specular = light["specular"];
        if (specular) {
            lightPtr->setSpecularColor(parseVec4(specular));
        }

        scene.addLight(std::move(lightPtr));

    }

}

void SceneLoader::loadCameras(Node cameras, View& view) {

    // To correctly match the behavior of the webGL library we'll need a place to store multiple view instances.
    // Since we only have one global view right now, we'll just apply the settings from the first active camera we find.

    for (auto it = cameras.begin(); it != cameras.end(); ++it) {

        const Node camera = it->second;

        const std::string type = camera["type"].as<std::string>();
        if (type == "perspective") {
            // The default camera
            Node fov = camera["fov"];
            if (fov) {
                // TODO
            }

            Node focal = camera["focal_length"];
            if (focal) {
                // TODO
            }

            Node vanishing = camera["vanishing_point"];
            if (vanishing) {
                // TODO
            }
        } else if (type == "isometric") {
            // TODO
            Node axis = camera["axis"];
            if (axis) {
                // TODO
            }
        } else if (type == "flat") {
            // TODO
        }

        double x = -74.00976419448854;
        double y = 40.70532700869127;
        float z = 16;

        Node position = camera["position"];
        if (position) {
            x = position[0].as<double>();
            y = position[1].as<double>();
            if (position.size() > 2) {
                z = position[2].as<float>();
            }
        }
        glm::dvec2 projPos = view.getMapProjection().LonLatToMeters(glm::dvec2(x, y));
        view.setPosition(projPos.x, projPos.y);

        Node zoom = camera["zoom"];
        if (zoom) {
            z = zoom.as<float>();
        }
        view.setZoom(z);

        Node active = camera["active"];
        if (active) {
            break;
        }

    }

}

Filter* SceneLoader::generateFilter(YAML::Node _filter) {

    std::vector<Filter*> filters;

    for(YAML::const_iterator filtItr = _filter.begin(); filtItr != _filter.end(); ++filtItr) {

        Filter* filter;

        if(_filter.IsSequence()) {

            filter = generateFilter(*filtItr);

        } else if(filtItr->first.as<std::string>() == "none") {

            filter = generateNoneFilter(_filter["none"]);

        } else if(filtItr->first.as<std::string>() == "not") {

            filter = generateNoneFilter(_filter["not"]);

        } else if (filtItr->first.as<std::string>() == "any") {

            filter = generateAnyFilter(_filter["any"]);

        } else if (filtItr->first.as<std::string>() == "all") {

            filter = generateFilter(_filter["all"]);

        } else {

            std::string key = filtItr->first.as<std::string>();
            filter = generatePredicate(_filter[key], key);

        }

        filters.push_back(filter);

    }

    if(filters.size() == 1) {
        return filters.front();
    } else if(filters.size() > 0) {
        return (new All(filters));
    } else {
        return (new Filter());
    }

}

Filter* SceneLoader::generatePredicate(YAML::Node _node, std::string _key) {

    if(_node.IsScalar()) {
        try {
            return (new Equality(_key, { new NumValue(_node.as<float>(), _node.as<std::string>()) }));
        } catch(const BadConversion& e) {
            std::string value = _node.as<std::string>();
            if(value == "true") {
                return (new Existence(_key, true));
            } else if(value == "false") {
                return (new Existence(_key, false));
            } else {
                return (new Equality(_key, {new StrValue(value)}));
            }
        }
    } else if(_node.IsSequence()) {
        ValueList values;
        for(YAML::const_iterator valItr = _node.begin(); valItr != _node.end(); ++valItr) {
            try {
                values.emplace_back(new NumValue(valItr->as<float>(), valItr->as<std::string>()));
            } catch(const BadConversion& e) {
                std::string value = valItr->as<std::string>();
                values.emplace_back(new StrValue(value));
            }
        }
        return (new Equality(_key, std::move(values)));
    } else if(_node.IsMap()) {
        float minVal = -std::numeric_limits<float>::infinity();
        float maxVal = std::numeric_limits<float>::infinity();

        for(YAML::const_iterator valItr = _node.begin(); valItr != _node.end(); ++valItr) {
            if(valItr->first.as<std::string>() == "min") {
                try {
                    minVal = valItr->second.as<float>();
                } catch(const BadConversion& e) {
                    logMsg("Error: Badly formed filter.\tExpect a float value type, string found.\n");
                    return (new Filter());
                }
            } else if(valItr->first.as<std::string>() == "max") {
                try {
                    maxVal = valItr->second.as<float>();
                } catch(const BadConversion& e) {
                    logMsg("Error: Badly formed filter.\tExpect a float value type, string found.\n");
                    return (new Filter());
                }
            } else {
                logMsg("Error: Badly formed Filter\n");
                return (new Filter());
            }
        }
        return (new Range(_key, minVal, maxVal));

    } else {
        logMsg("Error: Badly formed Filter\n");
        return (new Filter());
    }

}

Filter* SceneLoader::generateAnyFilter(YAML::Node _filter) {
    std::vector<Filter*> filters;

    if(!_filter.IsSequence()) {
        logMsg("Error: Badly formed filter. \"Any\" expects a list.\n");
        return (new Filter());
    }
    for(YAML::const_iterator filtItr = _filter.begin(); filtItr != _filter.end(); ++filtItr) {
        filters.emplace_back(generateFilter(*filtItr));
    }
    return (new Any(std::move(filters)));
}

Filter* SceneLoader::generateNoneFilter(YAML::Node _filter) {

    std::vector<Filter*> filters;

    if(_filter.IsSequence()) {
        for(YAML::const_iterator filtIter = _filter.begin(); filtIter != _filter.end(); ++filtIter) {
            filters.emplace_back(generateFilter(*filtIter));
        }
    } else if(_filter.IsMap()) { // not case
        for(YAML::const_iterator filtIter = _filter.begin(); filtIter != _filter.end(); ++filtIter) {
            std::string key = filtIter->first.as<std::string>();
            filters.emplace_back(generatePredicate(_filter[key], key));
        }
    } else {
        logMsg("Error: Badly formed filter. \"None\" expects a list or an object.\n");
        return (new Filter());
    }

    return (new None(std::move(filters)));
}

void SceneLoader::loadLayers(Node layers, Scene& scene, TileManager& tileManager) {

    if (!layers) {
        return;
    }

    for (auto layerIt = layers.begin(); layerIt != layers.end(); ++layerIt) {

        std::string name = layerIt->first.as<std::string>();
        Node drawGroup = layerIt->second["draw"];
        Node data = layerIt->second["data"];

        // TODO: handle data.source

        Node dataLayer = data["layer"];
        if (dataLayer) { name = dataLayer.as<std::string>(); }

        for (auto groupIt = drawGroup.begin(); groupIt != drawGroup.end(); ++groupIt) {

            StyleParams params;
            std::string styleName = groupIt->first.as<std::string>();
            Node styleProps = groupIt->second;

            Node order = styleProps["order"];
            if (order) { params.order = order.as<float>(); }

            Node color = styleProps["color"];
            if (color) {
                glm::vec4 c = parseVec4(color);
                params.color = (uint32_t(c.a * 255) << 24) +
                               (uint32_t(c.b * 255) << 16) +
                               (uint32_t(c.g * 255) << 8)  +
                               (uint32_t(c.r * 255));
                // TODO: color helper funtions
            }

            Node width = styleProps["width"];
            if (width) { params.width = width.as<float>(); }

            Node cap = styleProps["cap"];
            if (cap) {
                std::string capString = cap.as<std::string>();
                if (capString == "butt") { params.line.cap = CapTypes::BUTT; }
                else if (capString == "sqaure") { params.line.cap = CapTypes::SQUARE; }
                else if (capString == "round") { params.line.cap = CapTypes::ROUND; }
            }

            Node join = styleProps["join"];
            if (join) {
                std::string joinString = join.as<std::string>();
                if (joinString == "bevel") { params.line.join = JoinTypes::BEVEL; }
                else if (joinString == "miter") { params.line.join = JoinTypes::MITER; }
                else if (joinString == "round") { params.line.join = JoinTypes::ROUND; }
            }

            Node outline = styleProps["outline"];
            if (outline) {

                params.outline.on = true;

                Node color = outline["color"];
                if (color) {
                    glm::vec4 c = parseVec4(color);
                    params.outline.color = (uint32_t(c.a * 255) << 24) +
                    (uint32_t(c.b * 255) << 16) +
                    (uint32_t(c.g * 255) << 8)  +
                    (uint32_t(c.r * 255));
                    // TODO: color helper funtions
                }

                Node width = outline["width"];
                if (width) { params.outline.width = width.as<float>(); }

                Node cap = outline["cap"];
                if (cap) {
                    std::string capString = cap.as<std::string>();
                    if (capString == "butt") { params.outline.line.cap = CapTypes::BUTT; }
                    else if (capString == "sqaure") { params.outline.line.cap = CapTypes::SQUARE; }
                    else if (capString == "round") { params.outline.line.cap = CapTypes::ROUND; }
                }

                Node join = outline["join"];
                if (join) {
                    std::string joinString = join.as<std::string>();
                    if (joinString == "bevel") { params.outline.line.join = JoinTypes::BEVEL; }
                    else if (joinString == "miter") { params.outline.line.join = JoinTypes::MITER; }
                    else if (joinString == "round") { params.outline.line.join = JoinTypes::ROUND; }
                }

            }

            // match layer to the style in scene with the given name
            for (auto& style : scene.getStyles()) {
                if (style->getName() == styleName) {
                    style->addLayer({ name, params });
                }
            }

        }

    }

    // tileManager isn't used yet, but we'll need it soon to get the list of data sources

}
