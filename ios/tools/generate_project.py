"""Generate a self-contained Xcode project; no XcodeGen/CocoaPods required."""
from pathlib import Path
import hashlib
import json
import re
import xml.etree.ElementTree as ET

IOS = Path(__file__).resolve().parents[1]


def generate():
    objects = {}
    def ident(label): return hashlib.sha1(('zhanguo-ios:' + label).encode()).hexdigest()[:24].upper()
    def add(label, isa, **fields):
        key = ident(label)
        if key in objects: raise ValueError('Duplicate object: ' + label)
        objects[key] = dict(isa=isa, **fields)
        return key
    def file(path, type_, tree='SOURCE_ROOT'):
        return add('file:'+path, 'PBXFileReference', lastKnownFileType=type_, path=path, sourceTree=tree)
    def build(label, reference): return add('build:'+label, 'PBXBuildFile', fileRef=reference)
    sources = sorted(IOS.joinpath('Zhanguo/Sources').rglob('*.swift'))
    tests = sorted(IOS.joinpath('ZhanguoTests').rglob('*.swift'))
    assert sources and tests
    source_refs = [file(p.relative_to(IOS).as_posix(), 'sourcecode.swift') for p in sources]
    test_refs = [file(p.relative_to(IOS).as_posix(), 'sourcecode.swift') for p in tests]
    resource_specs = [('Zhanguo/Resources/GameAssets','folder'),
        ('Zhanguo/Resources/GameOverlay','folder'), ('Zhanguo/Resources/NativeConfig.json','text.json'),
        ('Zhanguo/Resources/MicroClient.json','text.json'),
        ('Zhanguo/Resources/LaunchScreen.storyboard','file.storyboard')]
    resource_refs = [file(p,t) for p,t in resource_specs]
    framework_refs = [file('System/Library/Frameworks/'+name+'.framework','wrapper.framework','SDKROOT')
                      for name in ['UIKit','WebKit','Foundation','Network','Security']]
    framework_refs.append(file('usr/lib/libz.tbd','sourcecode.text-based-dylib-definition','SDKROOT'))
    info = file('Zhanguo/Info.plist','text.plist.xml')
    app_product = add('product:app','PBXFileReference',explicitFileType='wrapper.application',includeInIndex=0,
                      path='Zhanguo.app',sourceTree='BUILT_PRODUCTS_DIR')
    test_product = add('product:tests','PBXFileReference',explicitFileType='wrapper.cfbundle',includeInIndex=0,
                       path='ZhanguoTests.xctest',sourceTree='BUILT_PRODUCTS_DIR')
    groups = [add('group:sources','PBXGroup',children=source_refs,name='Sources',sourceTree='<group>'),
              add('group:tests','PBXGroup',children=test_refs,name='Tests',sourceTree='<group>'),
              add('group:resources','PBXGroup',children=resource_refs+[info],name='Resources',sourceTree='<group>'),
              add('group:frameworks','PBXGroup',children=framework_refs,name='Frameworks',sourceTree='<group>')]
    products = add('group:products','PBXGroup',children=[app_product,test_product],name='Products',sourceTree='<group>')
    main_group = add('group:main','PBXGroup',children=groups+[products],sourceTree='<group>')
    app_sources = add('phase:app:sources','PBXSourcesBuildPhase',buildActionMask=2147483647,
        files=[build('src:'+str(i),ref) for i,ref in enumerate(source_refs)],runOnlyForDeploymentPostprocessing=0)
    app_resources = add('phase:app:resources','PBXResourcesBuildPhase',buildActionMask=2147483647,
        files=[build('resource:'+str(i),ref) for i,ref in enumerate(resource_refs)],runOnlyForDeploymentPostprocessing=0)
    app_frameworks = add('phase:app:frameworks','PBXFrameworksBuildPhase',buildActionMask=2147483647,
        files=[build('framework:'+str(i),ref) for i,ref in enumerate(framework_refs)],runOnlyForDeploymentPostprocessing=0)
    test_sources = add('phase:test:sources','PBXSourcesBuildPhase',buildActionMask=2147483647,
        files=[build('test:'+str(i),ref) for i,ref in enumerate(test_refs)],runOnlyForDeploymentPostprocessing=0)
    test_frameworks = add('phase:test:frameworks','PBXFrameworksBuildPhase',buildActionMask=2147483647,
        files=[build('test-framework:'+str(i),ref) for i,ref in enumerate(framework_refs)],runOnlyForDeploymentPostprocessing=0)
    def configurations(label, kind):
        refs=[]
        for name in ['Debug','Release']:
            settings = {'SDKROOT':'iphoneos','IPHONEOS_DEPLOYMENT_TARGET':'16.4',
                'SWIFT_VERSION':'5.0','SWIFT_STRICT_CONCURRENCY':'minimal',
                'CLANG_ENABLE_MODULES':'YES','CLANG_ENABLE_OBJC_ARC':'YES',
                'ENABLE_USER_SCRIPT_SANDBOXING':'YES','SWIFT_EMIT_LOC_STRINGS':'NO',
                'SUPPORTED_PLATFORMS':'iphoneos iphonesimulator','SUPPORTS_MACCATALYST':'NO',
                'TARGETED_DEVICE_FAMILY':'1,2','CODE_SIGN_STYLE':'Automatic'}
            if kind == 'app':
                settings.update(PRODUCT_NAME='Zhanguo',PRODUCT_BUNDLE_IDENTIFIER='com.zhanguo.chapterone.ios',
                    INFOPLIST_FILE='Zhanguo/Info.plist',GENERATE_INFOPLIST_FILE='NO',
                    CURRENT_PROJECT_VERSION='119',MARKETING_VERSION='0.1.0',
                    LD_RUNPATH_SEARCH_PATHS=['$(inherited)','@executable_path/Frameworks'])
            if kind == 'test':
                settings.update(PRODUCT_NAME='ZhanguoTests',PRODUCT_BUNDLE_IDENTIFIER='com.zhanguo.chapterone.ios.tests',
                    GENERATE_INFOPLIST_FILE='YES',TEST_HOST='$(BUILT_PRODUCTS_DIR)/Zhanguo.app/Zhanguo',
                    BUNDLE_LOADER='$(TEST_HOST)',LD_RUNPATH_SEARCH_PATHS=['$(inherited)','@loader_path/Frameworks','@executable_path/Frameworks'])
            if name == 'Debug':
                settings.update(DEBUG_INFORMATION_FORMAT='dwarf',ENABLE_TESTABILITY='YES',
                    SWIFT_OPTIMIZATION_LEVEL='-Onone',SWIFT_ACTIVE_COMPILATION_CONDITIONS='DEBUG $(inherited)',ONLY_ACTIVE_ARCH='YES')
            else:
                settings.update(DEBUG_INFORMATION_FORMAT='dwarf-with-dsym',SWIFT_OPTIMIZATION_LEVEL='-O',
                    SWIFT_COMPILATION_MODE='wholemodule',ONLY_ACTIVE_ARCH='NO')
            refs.append(add('config:'+label+':'+name,'XCBuildConfiguration',buildSettings=settings,name=name))
        return add('configs:'+label,'XCConfigurationList',buildConfigurations=refs,
                   defaultConfigurationIsVisible=0,defaultConfigurationName='Release')
    app_configs=configurations('app','app'); test_configs=configurations('tests','test')
    project_configs=configurations('project','project')
    proxy=add('test:proxy','PBXContainerItemProxy',containerPortal=ident('project'),proxyType=1,
              remoteGlobalIDString=ident('target:app'),remoteInfo='Zhanguo')
    dependency=add('test:dependency','PBXTargetDependency',target=ident('target:app'),targetProxy=proxy)
    app=add('target:app','PBXNativeTarget',buildConfigurationList=app_configs,
        buildPhases=[app_sources,app_frameworks,app_resources],buildRules=[],dependencies=[],name='Zhanguo',
        productName='Zhanguo',productReference=app_product,productType='com.apple.product-type.application')
    test=add('target:tests','PBXNativeTarget',buildConfigurationList=test_configs,
        buildPhases=[test_sources,test_frameworks],buildRules=[],dependencies=[dependency],name='ZhanguoTests',
        productName='ZhanguoTests',productReference=test_product,productType='com.apple.product-type.bundle.unit-test')
    project=add('project','PBXProject',attributes={'BuildIndependentTargetsInParallel':'YES','LastUpgradeCheck':'1600',
        'TargetAttributes':{app:{'CreatedOnToolsVersion':'16.0'},test:{'CreatedOnToolsVersion':'16.0','TestTargetID':app}}},
        buildConfigurationList=project_configs,compatibilityVersion='Xcode 14.0',developmentRegion='zh_CN',
        hasScannedForEncodings=0,knownRegions=['zh_CN','en','Base'],mainGroup=main_group,productRefGroup=products,
        projectDirPath='',projectRoot='',targets=[app,test])
    def atom(value):
        if isinstance(value,dict): return '{\n'+''.join(atom(str(k))+' = '+atom(v)+';\n' for k,v in value.items())+'}'
        if isinstance(value,list): return '(\n'+''.join(atom(v)+',\n' for v in value)+')'
        if isinstance(value,int): return str(value)
        return json.dumps(value,ensure_ascii=False)
    document={'archiveVersion':1,'classes':{},'objectVersion':56,'objects':objects,'rootObject':project}
    output=IOS/'Zhanguo.xcodeproj'; output.mkdir(exist_ok=True)
    (output/'project.pbxproj').write_text('// !$*UTF8*$!\n'+atom(document)+'\n',encoding='utf-8')
    scheme=f'''<?xml version="1.0" encoding="UTF-8"?>
<Scheme LastUpgradeVersion="1600" version="1.3">
<BuildAction parallelizeBuildables="YES" buildImplicitDependencies="YES"><BuildActionEntries>
<BuildActionEntry buildForTesting="YES" buildForRunning="YES" buildForProfiling="YES" buildForArchiving="YES" buildForAnalyzing="YES"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{app}" BuildableName="Zhanguo.app" BlueprintName="Zhanguo" ReferencedContainer="container:Zhanguo.xcodeproj"/></BuildActionEntry>
</BuildActionEntries></BuildAction>
<TestAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB" shouldUseLaunchSchemeArgsEnv="NO"><Testables><TestableReference skipped="NO"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{test}" BuildableName="ZhanguoTests.xctest" BlueprintName="ZhanguoTests" ReferencedContainer="container:Zhanguo.xcodeproj"/></TestableReference></Testables><CommandLineArguments><CommandLineArgument argument="--zg-tests" isEnabled="YES"/></CommandLineArguments></TestAction>
<LaunchAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB" launchStyle="0" useCustomWorkingDirectory="NO" ignoresPersistentStateOnLaunch="NO" debugDocumentVersioning="YES" debugServiceExtension="internal" allowLocationSimulation="YES"><BuildableProductRunnable runnableDebuggingMode="0"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{app}" BuildableName="Zhanguo.app" BlueprintName="Zhanguo" ReferencedContainer="container:Zhanguo.xcodeproj"/></BuildableProductRunnable></LaunchAction>
<ProfileAction buildConfiguration="Release" shouldUseLaunchSchemeArgsEnv="YES" savedToolIdentifier="" useCustomWorkingDirectory="NO" debugDocumentVersioning="YES"><BuildableProductRunnable runnableDebuggingMode="0"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{app}" BuildableName="Zhanguo.app" BlueprintName="Zhanguo" ReferencedContainer="container:Zhanguo.xcodeproj"/></BuildableProductRunnable></ProfileAction>
<AnalyzeAction buildConfiguration="Debug"/><ArchiveAction buildConfiguration="Release" revealArchiveInOrganizer="YES"/>
</Scheme>'''
    ET.fromstring(scheme)
    schemes=output/'xcshareddata/xcschemes'; schemes.mkdir(parents=True,exist_ok=True)
    (schemes/'Zhanguo.xcscheme').write_text(scheme,encoding='utf-8')
    def refs(value):
        if isinstance(value,dict):
            for v in value.values():yield from refs(v)
        elif isinstance(value,list):
            for v in value:yield from refs(v)
        elif isinstance(value,str) and re.fullmatch('[0-9A-F]{24}',value):yield value
    assert all(ref in objects for ref in refs(document)), 'Dangling project reference'
    record={'swiftSources':[p.relative_to(IOS).as_posix() for p in sources],
            'swiftTests':[p.relative_to(IOS).as_posix() for p in tests], 'objectCount':len(objects),
            'deploymentTarget':'16.4','swiftVersion':'5.0','libzLinked':True,
            'xcodeCompilationExecuted':False,'reason':'Generated and structurally validated on Windows; Apple SDK not available'}
    destination=IOS/'tests/project-generation.json'; destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in record.items() if k not in ['swiftSources','swiftTests']},ensure_ascii=False))


if __name__ == '__main__': generate()
