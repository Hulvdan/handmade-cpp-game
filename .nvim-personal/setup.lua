-- =========================================================
-- Вспомогательная хрень
-- =========================================================
local opts = { remap = false, silent = true }

function run_command()
    return vim.g.hulvdan_run_command
end

function cli_command(cmd)
    return [[.venv\Scripts\python.exe cmd\cli.py ]] .. cmd
end

vim.g.telescope_search_and_replace_directory = "sources"

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

-- =========================================================
-- Кнопки работы с кодом
-- =========================================================

-- Фолдинг всех блоков, начинающихся с `#if 0`.
vim.keymap.set("n", "<leader>0", function()
    if vim.bo.filetype == "cpp" then
        vim.fn.execute("%g/^#if 0/silent!normal! zDzf%")
        vim.api.nvim_input("<C-o>")

        -- NOTE: пытался прикрутить region / endregion folding.
        --
        -- vim.fn.execute("g/^#pragma region/silent!normal! ia<esc>")
        -- vim.fn.execute("%g/^#pragma region/silent!normal! v/endregion<CR>zf")
        -- vim.fn.execute([[%g/^#pragma region/silent!normal! zf/#pragma endregion<CR> _]])
        -- vim.api.nvim_input("<C-o>")

        -- NOTE: пытался сделать так, чтобы при нахождении `if 1`,
        -- курсор бы прыгал на `#else` блок, и его бы закрывал.
        -- Но в случае, когда у `#if 1` нет `#else`, закрывался этот `#if 1`.
        --
        -- vim.fn.execute("%g/^#if 1/silent!normal! %zDzf%")
        -- vim.api.nvim_input("<C-o>")
    end
end, opts)

-- =========================================================
-- Кнопки работы с проектом
-- =========================================================

-- Линтинг.
vim.keymap.set("n", "<leader>l", function()
    run_command()(cli_command("lint"))
end, opts)

-- Билд дебага.
vim.keymap.set("n", "<A-b>", function()
    run_command()(cli_command("build_game"))
end, opts)

-- Билд релиза.
vim.keymap.set("n", "<A-S-b>", function()
    run_command()(cli_command("release_game"))
end, opts)

-- Кодген.
vim.keymap.set("n", "<A-g>", function()
    run_command()(cli_command("generate"))
end, opts)

-- Пересоздание файлов VS.
vim.keymap.set("n", "<C-S-b>", function()
    run_command()(cli_command("cmake_vs_files"))
end, opts)

-- Билд + переключение окна на VS с одновременным запуском проекта.
vim.keymap.set("n", "<f5>", function()
    run_command()(cli_command("stoopid_windows_visual_studio_run"))
end, opts)

-- Билд релиза + переключение окна на VS с одновременным запуском проекта.
vim.keymap.set("n", "<S-f5>", function()
    run_command()(cli_command("stoopid_windows_visual_studio_run_release"))
end, opts)

-- Тесты.
vim.keymap.set("n", "<A-t>", function()
    run_command()(cli_command("test"))
end, opts)

-- Тестирование компиляции шейдеров.
vim.keymap.set("n", "<A-S-t>", function()
    run_command()(cli_command("test_shaders"))
end, opts)

-- Отформатировать C++ код в репозитории.
vim.keymap.set("n", "<A-S-f>", function()
    run_command()(cli_command("format"))
end, opts)
