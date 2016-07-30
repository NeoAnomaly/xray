		local amdopenclpath = os.getenv("AMDAPPSDKROOT")
		if( amdopenclpath ) then
			amdopenclpath = string.gsub(amdopenclpath, "\\", "/");
			local clIncludePath = string.format("%s/include",amdopenclpath);
			local clLibPath = string.format("%s/lib/x86",amdopenclpath);
			local clLibPath64 = string.format("%s/lib/x86_64",amdopenclpath);
			print("OpenCL_Path(AMD)", amdopenclpath)
			if (amdopenclpath) then
				includedirs {clIncludePath}
		--		libdirs {clLibPath}
				configuration "x64"
					libdirs {clLibPath64}
				configuration "x32"
					libdirs {clLibPath}
				configuration {}
				links {"OpenCL"}
			end
		end
		
		local nvidiaopenclpath = os.getenv("CUDA_PATH")
		if( nvidiaopenclpath ) then
			defines { "OPENCL_10" }
			nvidiaopenclpath = string.gsub(nvidiaopenclpath, "\\", "/");
			local clIncludePath = string.format("%s/include",nvidiaopenclpath);
			local clLibPath = string.format("%s/lib/Win32",nvidiaopenclpath);
			local clLibPath64 = string.format("%s/lib/x64",nvidiaopenclpath);
			print("OpenCL_Path(Nvidia)", nvidiaopenclpath)
			if (nvidiaopenclpath) then
				includedirs {clIncludePath}
		--		libdirs {clLibPath}
				configuration "x64"
					libdirs {clLibPath64}
				configuration "x32"
					libdirs {clLibPath}
				configuration {}
				links {"OpenCL"}
			end
		end
		
		local intelopenclpath = os.getenv("INTELOCLSDKROOT")
		if( intelopenclpath ) then
			defines { "INTEL_BUG_WORKROUND" }
			intelopenclpath = string.gsub(intelopenclpath, "\\", "/");
			local clIncludePath = string.format("%s/include",intelopenclpath);
			local clLibPath = string.format("%s/lib/x86",intelopenclpath);
			local clLibPath64 = string.format("%s/lib/x64",intelopenclpath);
			print("OpenCL_Path(Intel)", intelopenclpath)
			if (intelopenclpath) then
				includedirs {clIncludePath}
		--		libdirs {clLibPath}
				configuration "x64"
					libdirs {clLibPath64}
				configuration "x32"
					libdirs {clLibPath}
				configuration {}
				links {"OpenCL"}
			end
		end



