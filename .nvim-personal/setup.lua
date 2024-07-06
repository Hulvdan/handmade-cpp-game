-- Workspace Settings --
-- ================== --
vim.fn.execute(":set nornu")
vim.fn.execute(":set nonumber")
vim.fn.execute(":set signcolumn=no")
vim.fn.execute(":set colorcolumn=")
vim.fn.execute(":set nobackup")
vim.fn.execute(":set nowritebackup")

-- Helper Functions --
-- ================ --
local overseer = require("overseer")

function overseer_run(cmd)
    vim.fn.execute(":wa")

    overseer
        .new_task({
            cmd = cmd,
            components = {
                { "on_output_quickfix", open = true, close = true },
                { "on_exit_set_status", success_codes = { 0 } },
                "default",
            },
        })
        :start()
end

function cli_command(cmd)
    return [[.venv\Scripts\python.exe cmd\cli.py ]] .. cmd
end

-- Keyboard Shortcuts --
-- ================== --
local opts = { remap = false, silent = true }

vim.keymap.set("n", "<leader>l", function()
    overseer_run(cli_command("lint"))
end, opts)

vim.keymap.set("n", "<A-b>", function()
    overseer_run(cli_command("build_game"))
end, opts)

vim.keymap.set("n", "<A-g>", function()
    overseer_run(cli_command("generate"))
end, opts)

vim.keymap.set("n", "<C-S-b>", function()
    overseer_run(cli_command("cmake_vs_files"))
end, opts)

vim.keymap.set("n", "<f5>", function()
    overseer_run(cli_command("stoopid_windows_visual_studio_run"))
end, opts)

vim.keymap.set("n", "<A-t>", function()
    overseer_run(cli_command("test"))
end, opts)

vim.keymap.set("n", "<A-S-t>", function()
    overseer_run(cli_command("test_shaders"))
end, opts)

-- Thanks to https://forums.handmadehero.org/index.php/forum?view=topic&catid=4&id=704#3982
-- error message formats
-- Microsoft MSBuild
vim.fn.execute([[set errorformat+=\\\ %#%f(%l\\\,%c):\ %m]])
-- Microsoft compiler: cl.exe
vim.fn.execute([[set errorformat+=\\\ %#%f(%l)\ :\ %#%t%[A-z]%#\ %m]])
-- Microsoft HLSL compiler: fxc.exe
vim.fn.execute([[set errorformat+=\\\ %#%f(%l\\\,%c-%*[0-9]):\ %#%t%[A-z]%#\ %m]])

-- NOTE: Для обработки ошибок FlatBuffers
vim.fn.execute([[set errorformat+=\ \ %f(%l\\,\ %c\\):\ %m]])

require("conform").setup({
    formatters = {
        black = { command = [[.venv\Scripts\black.exe]] },
        isort = { command = [[.venv\Scripts\isort.exe]] },
    },
})
