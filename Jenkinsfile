#!/usr/bin/env groovy

// This pipeline is designed to run on Esri-internal CI infrastructure.

import groovy.transform.Field

@Library('psl')
import com.esri.zrh.jenkins.PipelineSupportLibrary

import com.esri.zrh.jenkins.JenkinsTools
import com.esri.zrh.jenkins.ce.CityEnginePipelineLibrary
import com.esri.zrh.jenkins.ce.PrtAppPipelineLibrary
import com.esri.zrh.jenkins.PslFactory
import com.esri.zrh.jenkins.psl.UploadTrackingPsl

@Field def psl = PslFactory.create(this, UploadTrackingPsl.ID)
@Field def cepl = new CityEnginePipelineLibrary(this, psl)
@Field def papl = new PrtAppPipelineLibrary(cepl)


// -- GLOBAL DEFINITIONS

@Field final String REPO         = 'https://github.com/Esri/palladio.git'
@Field final String SOURCE       = "palladio.git/src"
@Field final String SOURCE_STASH = 'palladio-sources'
@Field final String BUILD_TARGET = 'package'

@Field final List CONFIGS_PREPARE = [
	[ os: cepl.CFG_OS_RHEL7 ],
]

@Field final List CONFIGS_HOUDINI_180 = [
	[ os: cepl.CFG_OS_RHEL7, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_GCC93, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '18.0' ],
	[ os: cepl.CFG_OS_WIN10, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_VC1427, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '18.0' ],
]

@Field final List CONFIGS_HOUDINI_185 = [
	[ os: cepl.CFG_OS_RHEL7, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_GCC93, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '18.5' ],
	[ os: cepl.CFG_OS_WIN10, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_VC1427, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '18.5' ],
]

@Field final List CONFIGS_HOUDINI_190 = [
	[ os: cepl.CFG_OS_RHEL7, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_GCC93, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '19.0' ],
	[ os: cepl.CFG_OS_WIN10, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_VC1427, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '19.0' ],
]


// -- SETUP

properties([
	parameters([
		string(name: 'PRM_CESDK_BRANCH', defaultValue: '')
	]),
	disableConcurrentBuilds()
])

psl.runsHere('production')
env.PIPELINE_ARCHIVING_ALLOWED = "true"


// -- PIPELINE

stage('prepare') {
	cepl.runParallel(taskGenPrepare())
}

stage('build') {
	cepl.runParallel(taskGenBuild())
}

papl.finalizeRun('palladio', env.BRANCH_NAME)


// -- TASK GENERATORS

Map taskGenPrepare() {
	Map tasks = [:]
	tasks << cepl.generateTasks('prepare', this.&taskPrepare, CONFIGS_PREPARE)
	return tasks
}

Map taskGenBuild() {
    Map tasks = [:]
    // FIXME: this is a workaround to get unique task names
	tasks << cepl.generateTasks('pld-hdn18.0', this.&taskBuildPalladio, CONFIGS_HOUDINI_180)
	tasks << cepl.generateTasks('pld-hdn18.5', this.&taskBuildPalladio, CONFIGS_HOUDINI_185)
	tasks << cepl.generateTasks('pld-hdn19.0', this.&taskBuildPalladio, CONFIGS_HOUDINI_190)
	return tasks;
}


// -- TASK BUILDERS

def taskPrepare(cfg) {
 	cepl.cleanCurrentDir()
	papl.checkout(REPO, env.BRANCH_NAME)
	stash(name: SOURCE_STASH)
}

def taskBuildPalladio(cfg) {
	List deps = [] // empty dependencies = by default use conan packages

	List defs = [
		[ key: 'HOUDINI_USER_PATH',   val: "${env.WORKSPACE}/install" ],
		[ key: 'PLD_VERSION_BUILD',   val: env.BUILD_NUMBER ],
		[ key: 'PLD_HOUDINI_VERSION', val: cfg.houdini]
	]

	// pipeline params tell us if we need to build with a development version of CESDK
	if (params.PRM_CESDK_BRANCH) {
		// redirect the CESDK dependency to the right internal branch build
		Map myCESDK = PrtAppPipelineLibrary.Dependencies.CESDK.clone()
		myCESDK.g = { return params.PRM_CESDK_BRANCH.replace('/', '.') }
		deps << myCESDK

		defs << [ key: 'PLD_CONAN_CESDK_DIR', val: myCESDK.p ]
	}

	cepl.cleanCurrentDir()
	unstash(name: SOURCE_STASH)

	deps.each { d -> papl.fetchDependency(d, cfg) }
	dir(path: 'build') {
		final String stdOut = papl.runCMakeBuild(SOURCE, BUILD_TARGET, cfg, defs)
	}

	// dump build log to file for warnings scanner
	final String buildSuf = "${cepl.prtBuildSuffix(cfg)}-${cfg.houdini.replace('.', '_')}"
	final String buildLog = "build-${buildSuf}.log"
	writeFile(file: buildLog, text: stdOut)

	def versionExtractor = { p ->
		def vers = (p =~ /.*palladio-(.*)\.hdn.*/)
		return vers[0][1]
	}
	def classifierExtractor = { p ->
		def cls = (p =~ /.*palladio-.*\.(hdn.*)-(windows|linux)\..*/)
		return cls[0][1] + '.' + cepl.getArchiveClassifier(cfg)
	}
	papl.publish('palladio', env.BRANCH_NAME, "palladio-*", versionExtractor, cfg, classifierExtractor)

	// scan for compiler warnings
	final String idSuf = "${cfg.houdini.replace('.', '_')}-${cepl.getArchiveClassifier(cfg)}"
	if(cepl.isGCC(cfg)) {
		def gccReports = scanForIssues(tool: gcc4(pattern: buildLog), blameDisabled: true)
		publishIssues(id: "palladio-warnings-${idSuf}", name: "palladio-${idSuf}", issues: [gccReports])
	}
	else if(cepl.isMSVC(cfg)) {
		def msvcReports = scanForIssues(tool: msBuild(pattern: buildLog), blameDisabled: true)
		publishIssues(id: "palladio-warnings-${idSuf}", name: "palladio-${idSuf}", issues: [msvcReports])
	}
}
