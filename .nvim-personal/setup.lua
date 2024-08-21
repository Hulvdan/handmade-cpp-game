------------------------------------------------------------------------------------
-- Вспомогательная хрень.
------------------------------------------------------------------------------------
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

------------------------------------------------------------------------------------
-- Кнопки работы с кодом.
------------------------------------------------------------------------------------

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

vim.keymap.set("n", "<leader><leader>*", function()
    vim.api.nvim_input("ds)hx")
end, opts)

vim.keymap.set("v", "<leader>*", function()
    local _, ls, cs = unpack(vim.fn.getpos("v"))

    local _, le, ce = unpack(vim.fn.getpos("."))
    if ls > le then
        t = le
        le = ls
        ls = t
        t = cs
        cs = ce
        ce = t
    end
    if ls == le then
        if cs > ce then
            t = cs
            cs = ce
            ce = t
        end
    end

    vim.api.nvim_input(
        string.format(
            "<ESC>" --
                .. ":call cursor(%d,%d)<CR>"
                .. "i*(<ESC>"
                .. ":call cursor(%d,%d)<CR>"
                .. "i)<ESC>"
                .. ":call cursor(%d,%d)<CR>",
            ls,
            cs,
            le,
            ce + 3,
            ls,
            cs + 2
        )
    )
end, opts)

------------------------------------------------------------------------------------
-- Кнопки работы с проектом
------------------------------------------------------------------------------------

-- Линтинг.
vim.keymap.set("n", "<leader>l", function()
    run_command()(cli_command("lint"))
end, opts)

-- Билд дебага.
vim.keymap.set("n", "<A-b>", function()
    run_command()(cli_command("build_game_debug"))
end, opts)

-- Билд релиза.
vim.keymap.set("n", "<A-S-b>", function()
    run_command()(cli_command("build_game_release"))
end, opts)

-- Кодген.
vim.keymap.set("n", "<A-g>", function()
    run_command()(cli_command("generate"))
end, opts)

-- Билд + переключение окна на VS с одновременным запуском проекта.
vim.keymap.set("n", "<f5>", function()
    run_command()(cli_command("stoopid_windows_visual_studio_run_debug"))
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
