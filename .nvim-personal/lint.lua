local severity_map = {
    ["error"] = vim.diagnostic.severity.ERROR,
    ["warning"] = vim.diagnostic.severity.WARN,
    ["information"] = vim.diagnostic.severity.INFO,
    ["hint"] = vim.diagnostic.severity.HINT,
}

local pattern = "[^:]+:(%d+):(%d+):( %w+):(.+)"
local groups = { "lnum", "col", "code", "message" }
parser1 = require('lint.parser').from_pattern(pattern, groups, severity_map, {["source"] = "myclangtidy"}, {})

require("lint").linters.myclangtidy = {
      cmd = [[C:\Program Files\LLVM\bin\clang-tidy.exe]],
      stdin = false,
      append_fname = true,
      args = {"-checks=*", "--quiet"},
      stream = "stdout", -- ('stdout' | 'stderr' | 'both')
      ignore_exitcode = true, -- true = a code != 0 considered normal.
      env = nil,
      parser = parser1
}

require("lint").linters_by_ft = {
  cpp = {"myclangtidy",}
}

local ns = require("lint").get_namespace("myclangtidy")
vim.diagnostic.config({ virtual_text = true }, ns)
