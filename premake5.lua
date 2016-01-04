
-- Option to allow copying the DLL file to a custom folder after build
newoption {
	trigger = "copy-to",
	description = "Optional, copy the DLL to a custom folder after build, define the path here if wanted.",
	value = "PATH"
}

newoption {
	trigger = "no-new-structure",
	description = "Do not use new virtual path structure (separating headers and source files)."
}

newaction {
	trigger = "generate-buildinfo",
	description = "Sets up build information file like version.h.",
	onWorkspace = function(wks)
		-- get revision number via git
		local proc = assert(io.popen("git rev-list --count HEAD", "r"))
		local revNumber = assert(proc:read('*a')):gsub("%s+", "")
		proc:close()

		-- get old version number from version.hpp if any
		local oldRevNumber = "(none)"
		local oldVersionHeader = io.open(wks.location .. "/version.hpp", "r")
		if oldVersionHeader ~=nil then
			local oldVersionHeaderContent = assert(oldVersionHeader:read('*a'))
			oldRevNumber = string.match(oldVersionHeaderContent, "#define REVISION (%d+)")
			if oldRevNumber == nil then
				-- old version.hpp format?
				oldRevNumber = "(none)"
			end
		end

		-- generate version.hpp with a revision number if not equal
		if oldRevNumber ~= revNumber then
			print ("Update " .. oldRevNumber .. " -> " .. revNumber)
			local versionHeader = assert(io.open(wks.location .. "/version.hpp", "w"))
			versionHeader:write("/*\n")
			versionHeader:write(" * Automatically generated by premake5.\n")
			versionHeader:write(" * Do not touch, you fucking moron!\n")
			versionHeader:write(" */\n")
			versionHeader:write("\n")
			versionHeader:write("#define REVISION " .. revNumber .. "\n")
			versionHeader:close()
		end
	end
}

solution "iw4x"
	location ("./build")
	configurations { "Normal", "Debug", "DebugStatic", "Release", "ReleaseStatic" }

	project "iw4x"
		kind "SharedLib"
		language "C++"
		files { "./src/**.hpp", "./src/**.cpp" }
		includedirs { "%{prj.location}" }
		architecture "x32"
		configmap {
			["Normal"] = "Debug"
		}

		-- Allow newer Microsoft toolsets but fall back to VS2013
		if _ACTION == "vs2015" then
			toolset "msc-140"
		else
			toolset "msc-120"
		end

		configuration "Debug"
			defines { "DEBUG" }
			flags { "MultiProcessorCompile", "Symbols", "UndefinedIndentifiers", "No64BitChecks" }
			optimize "Debug"

		configuration "DebugStatic"
			defines { "NDEBUG" }
			flags { "MultiProcessorCompile", "Symbols", "UndefinedIndentifiers", "StaticRuntime", "No64BitChecks" }
			optimize "Debug"

		configuration "Release"
			defines { "NDEBUG" }
			flags { "MultiProcessorCompile", "Symbols", "UndefinedIndentifiers", "LinkTimeOptimization", "No64BitChecks" }
			optimize "Full"

		configuration "ReleaseStatic"
			defines { "NDEBUG" }
			flags { "MultiProcessorCompile", "Symbols", "UndefinedIndentifiers", "LinkTimeOptimization", "StaticRuntime", "No64BitChecks" }
			optimize "Full"

		if not _OPTIONS["no-new-structure"] then
			vpaths {
				["Headers/*"] = "src/**.hpp",
				["Sources/*"] = {"src/**.cpp"}
			}
		end

		vpaths {
			["Docs/*"] = {"**.txt","**.md"}
		}

		prebuildcommands {
			"cd %{_MAIN_SCRIPT_DIR}",
			"premake5 generate-buildinfo"
		}

		if _OPTIONS["copy-to"] then
			saneCopyToPath = string.gsub(_OPTIONS["copy-to"] .. "\\", "\\\\", "\\")
			postbuildcommands {
				"copy /y \"$(TargetPath)\" \"" .. saneCopyToPath .. "\""
			}
		end
