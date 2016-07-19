project "Calc"
    kind "StaticLib"
    location "."
	targetdir "$(SolutionDir)/_Lib/$(Configuration)/$(Platform)"
    includedirs { ".", "./inc", "../CLW" }
    files { "../Calc/**.h", "../Calc/**.cpp"}