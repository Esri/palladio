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
import com.esri.zrh.jenkins.ToolInfo

@Field def psl = PslFactory.create(this, UploadTrackingPsl.ID)
@Field def cepl = new CityEnginePipelineLibrary(this, psl)
@Field def papl = new PrtAppPipelineLibrary(cepl)


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
]

@Field final List INSTALLER_CONFIG = [ [ os: cepl.CFG_OS_WIN10, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_VC1427, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64 ] ]
@Field final List INSTALLER_HOUDINI_VERS = [ '18.5', '19.0', '19.5' ]

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

stage('installer') {
	cepl.runParallel(taskGenInstaller())
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

Map taskGenInstaller() {
    Map tasks = [:]
	tasks << cepl.generateTasks('pld-msi', this.&taskCreateInstaller, INSTALLER_CONFIG)
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
		papl.runCMakeBuild(SOURCE, 'build_and_run_tests', cfg, [], JenkinsTools.CMAKE319)
	}
	junit('build/test/palladio_test_report.xml')
}

def taskBuildPalladio(cfg) {
	List defs = [
		[ key: 'HOUDINI_USER_PATH',   val: "${env.WORKSPACE}/install" ],
		[ key: 'PLD_VERSION_BUILD',   val: env.BUILD_NUMBER ],
		[ key: 'PLD_HOUDINI_VERSION', val: cfg.houdini]
	]

	cepl.cleanCurrentDir()
	unstash(name: SOURCE_STASH)
	dir(path: 'build') {
		papl.runCMakeBuild(SOURCE, BUILD_TARGET, cfg, defs, JenkinsTools.CMAKE319)
	}
	final String artifactPattern = "palladio-*"

	if(cfg.os == cepl.CFG_OS_WIN10){
		dir(path: 'build'){
			stashFile = cepl.findOneFile(artifactPattern)
			echo("stashFile: '${stashFile}'")
			stash(includes: artifactPattern, name: "houdini${cfg.houdini.replaceAll('\\.', '_')}")
			stashPath = "${stashFile.path}"
			echo("file path to stash: '${stashPath}'")
		}
	}

	def versionExtractor = { p ->
		def vers = (p =~ /.*palladio-(.*)\.hdn.*/)
		return vers[0][1]
	}
	def classifierExtractor = { p ->
		def cls = (p =~ /.*palladio-.*\.(hdn.*)-(windows|linux)\..*/)
		return cls[0][1] + '.' + cepl.getArchiveClassifier(cfg)
	}
	papl.publish('palladio', env.BRANCH_NAME, artifactPattern, versionExtractor, cfg, classifierExtractor)
}

def taskCreateInstaller(cfg) {
	final String appName = 'palladio-installer'
	cepl.cleanCurrentDir()
	unstash(name: SOURCE_STASH)

	String pyArgs = ""

	// fetch outputs from builds
	dir(path: 'build'){
		dir(path: 'tmp'){
			INSTALLER_HOUDINI_VERS.each { mv ->
				unstash(name: "houdini${mv.replaceAll('\\.', '_')}")
				def zipFile = cepl.findOneFile("*hdn${mv.replaceAll('\\.', '-')}*.zip")
				final String zipFileName = zipFile.name
				unzip(zipFile:zipFileName)
				pyArgs += "-h${mv.replaceAll('\\.', '')} \"build\\tmp\\${zipFileName.take(zipFileName.lastIndexOf('.'))}\" "
			}
		}
	}
	pyArgs += "-bv ${env.BUILD_NUMBER} "
	pyArgs += "-bd \"build\\build_msi\" "

	// Toolchain definition for building MSI installers.
	final JenkinsTools compiler = cepl.getToolchainTool(cfg)
	final def toolchain = [
		new ToolInfo(JenkinsTools.WIX, cfg),
		new ToolInfo(compiler, cfg)
	]

	// Setup environment according to above toolchain definition.
	withTool(toolchain) {
		psl.runCmd('python "palladio.git\\deploy\\build.py" ' + pyArgs)

		def buildProps = papl.jpe.readProperties(file: 'build/build_msi/deployment.properties')

		final String artifactPattern = "build_msi/${buildProps.package_file}"
		final def artifactVersion = { p -> buildProps.package_version }

		def classifierExtractor = { p ->
			return cepl.getArchiveClassifier(cfg)
		}

		papl.publish(appName, env.BRANCH_NAME, artifactPattern, artifactVersion, cfg, classifierExtractor)
	}
}