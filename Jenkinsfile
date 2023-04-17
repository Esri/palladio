#!/usr/bin/env groovy

// This pipeline is designed to run on Esri-internal CI infrastructure.

import groovy.transform.Field

@Library('psl')
import com.esri.zrh.jenkins.PipelineSupportLibrary
import com.esri.zrh.jenkins.JenkinsTools
import com.esri.zrh.jenkins.ce.CityEnginePipelineLibrary
import com.esri.zrh.jenkins.ce.PrtAppPipelineLibrary as PAPL
import com.esri.zrh.jenkins.PslFactory
import com.esri.zrh.jenkins.psl.UploadTrackingPsl

@Field def psl = PslFactory.create(this, UploadTrackingPsl.ID)
@Field def cepl = new CityEnginePipelineLibrary(this, psl)
@Field def papl = new PAPL(cepl)


// -- GLOBAL DEFINITIONS

@Field final String REPO         = 'https://github.com/Esri/palladio.git'
@Field final String SOURCE       = "palladio.git/src"
@Field final String BUILD_TARGET = 'package'
@Field final String SOURCE_STASH = 'palladio-src'

@Field final List CONFIGS_CHECKOUT = [ [ ba: psl.BA_CHECKOUT ] ]

@Field final List CONFIGS_TEST = [
	[ os: cepl.CFG_OS_RHEL7, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_GCC93,  cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64 ],
	[ os: cepl.CFG_OS_WIN10, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_VC1427, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64 ],
]

@Field final List CONFIGS_HOUDINI_185 = [
	[ os: cepl.CFG_OS_RHEL7, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_GCC93, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '18.5' ],
	[ os: cepl.CFG_OS_WIN10, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_VC1427, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '18.5' ],
]

@Field final List CONFIGS_HOUDINI_190 = [
	[ os: cepl.CFG_OS_RHEL7, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_GCC93, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '19.0' ],
	[ os: cepl.CFG_OS_WIN10, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_VC1427, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '19.0' ],
]

@Field final List CONFIGS_HOUDINI_195 = [
	[ os: cepl.CFG_OS_RHEL7, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_GCC93, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '19.5' ],
	[ os: cepl.CFG_OS_WIN10, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_VC1427, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '19.5' ],
	[ grp: 'cesdkLatest', os: cepl.CFG_OS_RHEL7, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_GCC93, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '19.5', cesdk: PAPL.Dependencies.CESDK_LATEST ],
	[ grp: 'cesdkLatest', os: cepl.CFG_OS_WIN10, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_VC1427, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '19.5', cesdk: PAPL.Dependencies.CESDK_LATEST ],
]


// -- SETUP

psl.runsHere('production')
env.PIPELINE_ARCHIVING_ALLOWED = "true"
properties([ disableConcurrentBuilds() ])


// -- PIPELINE

stage('prepare'){
	cepl.runParallel(taskGenCheckout())
}

stage('test') {
	cepl.runParallel(taskGenTest())
}

stage('build') {
	cepl.runParallel(taskGenBuild())
}

papl.finalizeRun('palladio', env.BRANCH_NAME)


// -- TASK GENERATORS

Map taskGenCheckout(){
	Map tasks = [:]
	tasks << cepl.generateTasks('pld-src', this.&taskSourceCheckout, CONFIGS_CHECKOUT)
	return tasks
}

Map taskGenTest() {
    Map tasks = [:]
	tasks << cepl.generateTasks('pld-test', this.&taskRunTest, CONFIGS_TEST)
	return tasks;
}

Map taskGenBuild() {
    Map tasks = [:]
	tasks << cepl.generateTasks('pld-hdn18.5', this.&taskBuildPalladio, CONFIGS_HOUDINI_185)
	tasks << cepl.generateTasks('pld-hdn19.0', this.&taskBuildPalladio, CONFIGS_HOUDINI_190)
	tasks << cepl.generateTasks('pld-hdn19.5', this.&taskBuildPalladio, CONFIGS_HOUDINI_195)
	return tasks;
}


// -- TASK BUILDERS

def taskSourceCheckout(cfg) {
	cepl.cleanCurrentDir()
	papl.checkout(REPO, env.BRANCH_NAME)
	stash(name: SOURCE_STASH)
}

def taskRunTest(cfg) {
	cepl.cleanCurrentDir()
	unstash(name: SOURCE_STASH)
	dir(path: 'build') {
		papl.runCMakeBuild(SOURCE, 'build_and_run_tests', cfg, [])
	}
	junit('build/test/palladio_test_report.xml')
}

def taskBuildPalladio(cfg) {
	cepl.cleanCurrentDir()
	unstash(name: SOURCE_STASH)

	List defs = applyCeSdkOverride(cfg) + [
		[ key: 'HOUDINI_USER_PATH',   val: "${env.WORKSPACE}/install" ],
		[ key: 'PLD_VERSION_BUILD',   val: env.BUILD_NUMBER ],
		[ key: 'PLD_HOUDINI_VERSION', val: cfg.houdini]
	]
	dir(path: 'build') {
		final String stdOut = papl.runCMakeBuild(SOURCE, BUILD_TARGET, cfg, defs)
		scanAndPublishBuildIssues(cfg, stdOut)
	}

	def versionExtractor = { p ->
		def vers = (p =~ /.*palladio-(.*)\.hdn.*/)
		return vers[0][1]
	}
	def classifierExtractor = { p ->
		def cls = (p =~ /.*palladio-.*\.(hdn.*)-(windows|linux)\..*/)
		return cls[0][1] + '.' + cepl.getArchiveClassifier(cfg)
	}
	papl.publish('palladio', env.BRANCH_NAME, "palladio-*", versionExtractor, cfg, classifierExtractor)
}

def scanAndPublishBuildIssues(Map cfg, String consoleOut) {
	final String houdiniSuf = cfg.houdini.replace('.', '_')
	final String buildSuf = "${cepl.prtBuildSuffix(cfg)}-${houdiniSuf}"
	final String buildLog = "build-${buildSuf}.log"
	final String idSuf = (cfg.grp ? cfg.grp + "-" : "") + "${houdiniSuf}-${cepl.getArchiveClassifier(cfg)}"

	// dump build log to file for warnings scanner
	writeFile(file: buildLog, text: consoleOut)

	// scan for compiler warnings
	def scanReport = scanForIssues(tool: cepl.isGCC(cfg) ? gcc4(pattern: buildLog) : msBuild(pattern: buildLog), blameDisabled: true)
	publishIssues(id: "palladio-warnings-${idSuf}", name: "palladio-${idSuf}", issues: [scanReport])
}

List applyCeSdkOverride(cfg) {
	if (cfg.cesdk) {
		papl.fetchDependency(cfg.cesdk, cfg)
		return [ [ key: 'PLD_CESDK_DIR:PATH', val: cfg.cesdk.p ] ]
	}
	return []
}
