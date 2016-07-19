project "CLW"
    kind "StaticLib"
    location "."  
    includedirs { "." }
	targetdir "$(SolutionDir)/_Lib/$(Configuration)/$(Platform)"
    files { "../CLW/**.h", "../CLW/**.cpp", "../CLW/**.cl" }

    if _OPTIONS["embed_kernels"] then
		configuration {}
		defines {"FR_EMBED_KERNELS"}
		os.execute("python ../../../Tools/scripts/stringify.py ./CL/ > ./CL/cache/kernels.h")
		print ">> CLW: CL kernels embedded"
    end