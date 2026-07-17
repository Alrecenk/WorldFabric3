#pragma once
#ifndef _Glove_Glove_H_
#define _Glove_Glove_H_ 1


#include "WorldPlugin.h"
#include "ScenePlugin.h"
#include "PanelPlugin.h"
#include "glm/glm.hpp"
#include <stdio.h>
#include <cstdlib>
#include <Piece.h>

#include <string>
#include <map>
#include <set>

namespace Chess {


	class Glove : public WorldObject {
	public:

		std::string model_name;
		bool is_white = true;

		Glove(const glm::vec3& p, const std::string& model_name_set, const bool& is_white_arg = true);

		std::vector<std::vector<int64_t>> grid;
		void destroy();

		//Functions used on observables or on read objects need to be const
		void print() const override;

		void setPosition(const glm::vec3& p);

		Glove() {} // WorldObject's need a default constructor to make an object to deserialize into
		virtual ~Glove() = default; // Force to be polymorphic just in case

		//This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
		// Just change the template parameter to match your class
		int getTypeId(Registry* r) const {
			return r->getIdForType<Glove>();
		}

	};


	class GloveView : public ObjectView<Glove> {
	public:
		int64_t id = -1;
		int scene_id = -1;
		glm::mat4 pose;

		//created is called when an objectis observed that ws no observed last time view was called on the world
		void created(std::shared_ptr<const Glove>& observation) override;

		//Update is called when an observation is made of an object that was also observed last frame on this same view
		void updated(std::shared_ptr<const Glove>& observation) override;

		//Destroyed is called when an observation that was present in the last observation is no longer observed
		//This view will be deleted immediately after this call (it's destructor will be called after this)
		void destroyed() override;
	};

	auto static getStructure(Glove& obj) {
		return std::tie(obj.position, obj.model_name, obj.is_white);
	}


} // end Glove name space

#endif // #ifndef _Glove_Glove_H_