/*
 * DNAInteraction_relax.cpp
 *
 *  Created on: Mar 13, 2013
 *      Author: Ben Snodin
 */

#include <fstream>

#include "DNAInteraction_relax.h"
#include "../Particles/DNANucleotide.h"

DNAInteraction_relax::DNAInteraction_relax() :
				DNAInteraction() {
	OX_LOG(Logger::LOG_INFO, "Using unphysical backbone (DNA_relax interaction)");
	_constant_force = 0;
	_harmonic_force = 1;
}

DNAInteraction_relax::~DNAInteraction_relax() {
}

number DNAInteraction_relax::_backbone(BaseParticle *p, BaseParticle *q, bool compute_r, bool update_forces) {
	if(!_check_bonded_neighbour(&p, &q, compute_r)) {
		return (number) 0.f;
	}

	if(compute_r) {
		_computed_r = q->pos - p->pos;
	}

	LR_vector rback = _computed_r + q->int_centers[DNANucleotide::BACK] - p->int_centers[DNANucleotide::BACK];
	number rbackmod = rback.module();
	number rbackr0 = rbackmod - HARMONIC_R0;
	number energy = 0;
	if(_backbone_type == _constant_force) energy = _backbone_k * fabs(rbackr0);
	else if(_backbone_type == _harmonic_force) energy = 0.5 * _backbone_k * rbackr0 * rbackr0;

	if(update_forces) {
		LR_vector force;
		// this does not conserve energy very well -- but there is a discontinuity in the force, so...
		if(_backbone_type == _constant_force) {
			if(rbackr0 > 0) force = rback * (-_backbone_k / rbackmod);
			else force = rback * (_backbone_k / rbackmod);
		}
		// conserves energy about as well as the normal DNAInteraction
		else if(_backbone_type == _harmonic_force) force = rback * (-_backbone_k * rbackr0 / rbackmod);

		p->force -= force;
		q->force += force;

		_update_stress_tensor(_computed_r, force);

		// we need torques in the reference system of the particle
		p->torque -= p->orientationT * p->int_centers[DNANucleotide::BACK].cross(force);
		q->torque += q->orientationT * q->int_centers[DNANucleotide::BACK].cross(force);
	}

	return energy;
}

void DNAInteraction_relax::check_input_sanity(std::vector<BaseParticle *> &particles) {
	int N = particles.size();
	for(int i = 0; i < N; i++) {
		BaseParticle *p = particles[i];
		if(p->n3 != P_VIRTUAL && p->n3->index >= N) {
			throw oxDNAException("Wrong topology for particle %d (n3 neighbor is %d, should be < N = %d)", i, p->n3->index, N);
		}
		if(p->n5 != P_VIRTUAL && p->n5->index >= N) {
			throw oxDNAException("Wrong topology for particle %d (n5 neighbor is %d, should be < N = %d)", i, p->n5->index, N);
		}
	}
}

void DNAInteraction_relax::get_settings(input_file &inp) {
	DNAInteraction::get_settings(inp);

	char tmps[256];
	getInputString(&inp, "relax_type", tmps, 1);
	if(strcmp(tmps, "constant_force") == 0) _backbone_type = _constant_force;
	else if(strcmp(tmps, "harmonic_force") == 0) _backbone_type = _harmonic_force;
	else throw oxDNAException("Error while parsing input file: relax_type '%s' not implemented; use constant_force or harmonic_force", tmps);

	float ftmp;
	if(getInputFloat(&inp, "relax_strength", &ftmp, 0) == KEY_FOUND) {
		_backbone_k = (number) ftmp;
		OX_LOG(Logger::LOG_INFO, "Using spring constant = %f for the DNA_relax interaction", _backbone_k);
	}
	else {
		if (_backbone_type == _harmonic_force) {
			_backbone_k = (number) 32.;
		}
		else {
			_backbone_k = (number) 1.;
		}
		OX_LOG(Logger::LOG_INFO, "Using default strength constant = %f for the DNA_relax interaction", _backbone_k);
	}
}
