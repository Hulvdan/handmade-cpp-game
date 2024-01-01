-- TODO(hulvdan): Figure out why this pattern from source does not work
-- Source: https://github.com/mfussenegger/nvim-lint/blob/master/lua/lint/linters/clangtidy.lua
-- local pattern = [[([^:]*):(%d+):(%d+): (%w+): ([^[]+)]]
-- local groups = { 'file', 'lnum', 'col', 'severity', 'message' }
local pattern = "[^:]+:(%d+):(%d+): (%w+):(.+)"
local groups = { "lnum", "col", "severity", "message" }

local severity_map = {
    ["error"] = vim.diagnostic.severity.ERROR,
    ["warning"] = vim.diagnostic.severity.WARN,
    ["information"] = vim.diagnostic.severity.INFO,
    ["hint"] = vim.diagnostic.severity.HINT,
    ["note"] = vim.diagnostic.severity.HINT,
}

require("lint").linters.myclangtidy = {
    cmd = [[C:\Program Files\LLVM\bin\clang-tidy.exe]],
    stdin = false,
    append_fname = true,
    args = { "--quiet", "--config-file=.clang-tidy-minimal" },
    ignore_exitcode = true, -- true = a code != 0 considered normal.
    parser = require("lint.parser").from_pattern(
        pattern, groups, severity_map, { ["source"] = "myclangtidy" }
    )
}

require("lint").linters_by_ft = {
  cpp = {"myclangtidy",}
}

local ns = require("lint").get_namespace("myclangtidy")
vim.diagnostic.config({ virtual_text = false }, ns)

-- TODO(hulvdan): Move this to the main config
vim.fn.sign_define("DiagnosticSignWarn", { text = "", texthl = "DiagnosticSignWarn" })
vim.fn.sign_define("DiagnosticSignInfo", { text = "", texthl = "DiagnosticSignInfo" })
vim.fn.sign_define("DiagnosticSignHint", { text = "", texthl = "DiagnosticSignHint" })

vim.keymap.set("n", "<leader>l", function()
    vim.fn.execute("term cmd\\lint.bat")
    vim.fn.execute("wincmd x")
    vim.fn.execute("wincmd l")
end, opts)
