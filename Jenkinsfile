#!/usr/bin/env groovy

// This pipeline is designed to run on Esri-internal CI infrastructure.

import groovy.transform.Field


// -- PIPELINE LIBRARIES

@Library('psl')
import com.esri.zrh.jenkins.PipelineSupportLibrary 

@Field def psl = new PipelineSupportLibrary(this)


// -- SETUP

properties([
	parameters([
		string(name: 'PRM_CESDK_BRANCH', defaultValue: '')
	]),
	disableConcurrentBuilds()
])

psl.runsHere('production')
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
