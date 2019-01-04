#!/usr/bin/env groovy

// This pipeline is designed to run on Esri-internal CI infrastructure.

import groovy.transform.Field


// -- PIPELINE LIBRARIES

@Library('psl@simo6772/CE-6230-palladio-conan-python')
import com.esri.zrh.jenkins.PipelineSupportLibrary 

@Field def psl = new PipelineSupportLibrary(this)


// -- SETUP

properties([ disableConcurrentBuilds() ])
psl.runsHere('testing', 'development')
env.PIPELINE_ARCHIVING_ALLOWED = "true"


// -- LOAD & RUN PIPELINE

def impl

node {
	checkout scm
	impl = load('pipeline.groovy')
}

stage('palladio') {
	impl.pipeline()
}
