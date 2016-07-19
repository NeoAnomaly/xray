project "xrLC2"
	kind "ConsoleApp"
	location "."
	targetdir "$(SolutionDir)/Bin/$(Configuration)/$(Platform)"
	
	includedirs { "./include", "../CLW", "../Calc/inc" }
	links {"Calc", "CLW"}
	defines {"EXPORT_API"}
	files { "**.h", "**.cpp" }
