#!/usr/bin/env groovy

// This pipeline is designed to run on Esri-internal CI infrastructure.

import groovy.transform.Field

@Library('psl')
import com.esri.zrh.jenkins.PipelineSupportLibrary as PSL
import com.esri.zrh.jenkins.JenkinsTools
import com.esri.zrh.jenkins.ce.CityEnginePipelineLibrary as CEPL
import com.esri.zrh.jenkins.ce.PrtAppPipelineLibrary as PAPL
import com.esri.zrh.jenkins.PslFactory
import com.esri.zrh.jenkins.psl.UploadTrackingPsl

@Field def psl = PslFactory.create(this, UploadTrackingPsl.ID)
@Field def cepl = new CEPL(this, psl)
@Field def papl = new PAPL(cepl)


// -- GLOBAL DEFINITIONS

@Field final String REPO         = 'https://github.com/Esri/palladio.git'
@Field final String SOURCE       = "palladio.git/src"
@Field final String BUILD_TARGET = 'package'
@Field final String SOURCE_STASH = 'palladio-src'

@Field final List CONFIGS_CHECKOUT = [ [ ba: PSL.BA_CHECKOUT ] ]
@Field final Map DOCKER_IMAGE_LINUX_CONFIG = [ ba: PSL.BA_LINUX_DOCKER, containerId: "build_tools/ce-tc-prt:almalinux8-gcc11-v2", containerWorkspace: "/tmp/app" ]

@Field final List CONFIGS_TEST = [
	DOCKER_IMAGE_LINUX_CONFIG + [ os: CEPL.CFG_OS_RHEL8, bc: CEPL.CFG_BC_REL, tc: CEPL.CFG_TC_GCC112, cc: CEPL.CFG_CC_OPT, arch: CEPL.CFG_ARCH_X86_64 ],
	[ os: CEPL.CFG_OS_WIN10, bc: CEPL.CFG_BC_REL, tc: CEPL.CFG_TC_VC1437, cc: CEPL.CFG_CC_OPT, arch: CEPL.CFG_ARCH_X86_64 ],
]

@Field final List CONFIGS_HOUDINI_190 = [
	DOCKER_IMAGE_LINUX_CONFIG + [ os: CEPL.CFG_OS_RHEL8, bc: CEPL.CFG_BC_REL, tc: CEPL.CFG_TC_GCC112, cc: CEPL.CFG_CC_OPT, arch: CEPL.CFG_ARCH_X86_64, houdini: '19.0' ],
	[ os: CEPL.CFG_OS_WIN10, bc: CEPL.CFG_BC_REL, tc: CEPL.CFG_TC_VC1437, cc: CEPL.CFG_CC_OPT, arch: CEPL.CFG_ARCH_X86_64, houdini: '19.0' ],
]

@Field final List CONFIGS_HOUDINI_195 = [
	DOCKER_IMAGE_LINUX_CONFIG + [ os: CEPL.CFG_OS_RHEL8, bc: CEPL.CFG_BC_REL, tc: CEPL.CFG_TC_GCC112, cc: CEPL.CFG_CC_OPT, arch: CEPL.CFG_ARCH_X86_64, houdini: '19.5' ],
	[ os: CEPL.CFG_OS_WIN10, bc: CEPL.CFG_BC_REL, tc: CEPL.CFG_TC_VC1437, cc: CEPL.CFG_CC_OPT, arch: CEPL.CFG_ARCH_X86_64, houdini: '19.5' ],
]

@Field final List CONFIGS_HOUDINI_200 = [
	DOCKER_IMAGE_LINUX_CONFIG + [ os: CEPL.CFG_OS_RHEL8, bc: CEPL.CFG_BC_REL, tc: CEPL.CFG_TC_GCC112, cc: CEPL.CFG_CC_OPT, arch: CEPL.CFG_ARCH_X86_64, houdini: '20.0' ],
	[ os: CEPL.CFG_OS_WIN10, bc: CEPL.CFG_BC_REL, tc: CEPL.CFG_TC_VC1437, cc: CEPL.CFG_CC_OPT, arch: CEPL.CFG_ARCH_X86_64, houdini: '20.0' ],
	DOCKER_IMAGE_LINUX_CONFIG + [ grp: 'cesdkLatest', os: CEPL.CFG_OS_RHEL8, bc: CEPL.CFG_BC_REL, tc: CEPL.CFG_TC_GCC112, cc: CEPL.CFG_CC_OPT, arch: CEPL.CFG_ARCH_X86_64, houdini: '20.0', cesdk: PAPL.Dependencies.CESDK_LATEST ],
	[ grp: 'cesdkLatest', os: CEPL.CFG_OS_WIN10, bc: CEPL.CFG_BC_REL, tc: CEPL.CFG_TC_VC1437, cc: CEPL.CFG_CC_OPT, arch: CEPL.CFG_ARCH_X86_64, houdini: '20.0', cesdk: PAPL.Dependencies.CESDK_LATEST ],
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
	tasks << cepl.generateTasks('pld-hdn19.0', this.&taskBuildPalladio, CONFIGS_HOUDINI_190)
	tasks << cepl.generateTasks('pld-hdn19.5', this.&taskBuildPalladio, CONFIGS_HOUDINI_195)
	tasks << cepl.generateTasks('pld-hdn20.0', this.&taskBuildPalladio, CONFIGS_HOUDINI_200)
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
	buildPalladio(cfg, [], 'build_and_run_tests')
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

	final String stdOut = buildPalladio(cfg, defs, BUILD_TARGET)
	if(!papl.runsOnDocker(cfg)) {
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

def buildPalladio(cfg, defs, target) {
	if(cfg.os == CEPL.CFG_OS_RHEL8) {
		Map dirMap = [ "${env.WORKSPACE}" : cfg.containerWorkspace ]
		Map envMap = [ DEFAULT_UID: '$(id -u)', DEFAULT_GID: '$(id -g)', WORKSPACE: cfg.containerWorkspace ]
		String src = "${cfg.containerWorkspace}/${SOURCE}";
		String bld = "${cfg.containerWorkspace}/build";

		cmd = "cd ${src}"
		cmd += "\npython3 -m ensurepip"
		cmd += "\npython3 -m pip install --user pipenv"
		cmd += "\npython3 -m pipenv install"
		cmd += "\nPYVENV=\$(python3 -m pipenv --venv)"
		cmd += "\ncd ${cfg.containerWorkspace}"
		cmd += "\nsource \${PYVENV}/bin/activate"
		cmd += "\nconan remote add --force --insert=0 zrh-conan ${psl.CONAN_REMOTE_URL}"

		cmd += "\ncmake -G Ninja -DCMAKE_BUILD_TYPE=Release "
		defs.each { d -> cmd += " -D${d.key}=${ (d.val instanceof Closure) ? d.val.call(cfg) : d.val }" }
		cmd +=" -S ${src} -B ${bld}"

		cmd += "\ncmake --build ${bld} --target ${target}"
		String stdOut = psl.runDockerCmd(cfg.containerId, cfg.containerWorkspace, cmd, dirMap, envMap)
		echo(stdOut)
		return stdOut
	} else {
		return papl.runCMakeBuild(SOURCE, 'build', target, cfg, defs)
	}
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
