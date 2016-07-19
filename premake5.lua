function fileExists(name)
   local f=io.open(name,"r")
   if f~=nil then io.close(f) return true else return false end
end

workspace "xrCompilers2"
	configurations { "Debug", "Release" }
	platforms { "Win32", "x64" }
    language "C++"
    flags { "NoMinimalRebuild", "Symbols" }
	objdir "$(SolutionDir)/_TempBin"
    
	-- find and add path to Opencl headers
    dofile ("./OpenCLSearch.lua" )
    
	-- define common includes
    --includedirs { "./3rd Party/" }

	defines{ "WIN32", "_CRT_SECURE_NO_WARNINGS" }
	buildoptions { "/MP"  } --multiprocessor build

    --make configuration specific definitions
    filter "configurations:Debug"
        defines { "_DEBUG" }
    filter "configurations:Release"
        defines { "NDEBUG" }
        flags { "OptimizeSpeed" }
		
	filter "platforms:*32"
		architecture "x86"
	filter "platforms:*64"
		architecture "x86_64"
		
	filter { "configurations:Debug", "platforms:*32" }
		targetsuffix( "D" )
	filter { "configurations:Debug", "platforms:*64" }
		targetsuffix( "64D" )
	filter { "configurations:Release", "platforms:*64" }
		targetsuffix( "64" )
	filter {} -- reset filter
	
	--[[
    if fileExists("./xray/xrLC/xrLC.lua") then
        dofile("./xray/xrLC/xrLC.lua")
    end
    ]]
    
    if fileExists("./3rd party/OpenCL/CLW/CLW.lua") then
        dofile("./3rd party/OpenCL/CLW/CLW.lua")
    end

    if fileExists("./3rd party/OpenCL/Calc/Calc.lua") then
        dofile("./3rd party/OpenCL/Calc/Calc.lua")
    end