------------------------------------------------------------
--                 Вспомогательная хрень                  --
------------------------------------------------------------
run_command = vim.g.hulvdan_run_command

function cli_command(cmd)
    return [[.venv\Scripts\python.exe cmd\cli.py ]] .. cmd
end

-- Обработка ошибок MSBuild.
-- https://forums.handmadehero.org/index.php/forum?view=topic&catid=4&id=704#3982
-- Microsoft MSBuild
vim.fn.execute([[set errorformat+=\\\ %#%f(%l\\\,%c):\ %m]])
-- Microsoft compiler: cl.exe
vim.fn.execute([[set errorformat+=\\\ %#%f(%l)\ :\ %#%t%[A-z]%#\ %m]])
-- Microsoft HLSL compiler: fxc.exe
vim.fn.execute([[set errorformat+=\\\ %#%f(%l\\\,%c-%*[0-9]):\ %#%t%[A-z]%#\ %m]])

-- Обработка ошибок FlatBuffers.
vim.fn.execute([[set errorformat+=\ \ %f(%l\\,\ %c\\):\ %m]])

-- Форматтер.
require("conform").setup({
    formatters = {
        black = { command = [[.venv\Scripts\black.exe]] },
        isort = { command = [[.venv\Scripts\isort.exe]] },
    },
})

local opts = { remap = false, silent = true }

------------------------------------------------------------
--                 Кнопки работы с кодом                  --
------------------------------------------------------------

-- Фолдинг всех блоков, начинающихся с `#if 0`.
vim.keymap.set("n", "<leader>0", function()
    if vim.bo.filetype == "cpp" then
        vim.fn.execute("%g/^#if 0/silent!normal! zDzf%")
        vim.api.nvim_input("<C-o>")
    end
end)

------------------------------------------------------------
--                Кнопки работы с проектом                --
------------------------------------------------------------

-- Линтинг.
vim.keymap.set("n", "<leader>l", function()
    run_command(cli_command("lint"))
end, opts)

-- Билд.
vim.keymap.set("n", "<A-b>", function()
    run_command(cli_command("build_game"))
end, opts)

-- Кодген.
vim.keymap.set("n", "<A-g>", function()
    run_command(cli_command("generate"))
end, opts)

-- Пересоздание файлов VS.
vim.keymap.set("n", "<C-S-b>", function()
    run_command(cli_command("cmake_vs_files"))
end, opts)

-- Билд + переключение окна на VS с одновременным запуском проекта.
vim.keymap.set("n", "<f5>", function()
    run_command(cli_command("stoopid_windows_visual_studio_run"))
end, opts)

-- Тесты.
vim.keymap.set("n", "<A-t>", function()
    run_command(cli_command("test"))
end, opts)

-- Тестирование компиляции шейдеров.
vim.keymap.set("n", "<A-S-t>", function()
    run_command(cli_command("test_shaders"))
end, opts)

-- Отформатировать C++ код в репозитории.
vim.keymap.set("n", "<A-S-f>", function()
    run_command(cli_command("format"))
end, opts)
