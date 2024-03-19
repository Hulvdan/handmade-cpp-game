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
local todo_plugin = require("todo-comments");
local overseer = require("overseer")

-- Options:
-- go_down - default: true - Places a cursor at the bottom upon launching the command
function launch_tab(command, options)
    vim.g.hulvdan_DisableAnimations()

    options = options or { go_down = true }
    if options.go_down == nil then options.go_down = true end

    vim.fn.execute([[term ]] .. command)
    if options.go_down then
        vim.fn.execute("norm G")
    end

    vim.g.hulvdan_EnableAnimations()
end

-- Options:
-- go_down - default: true - Places a cursor at the bottom upon launching the command
function launch_side(command, options)
    vim.g.hulvdan_DisableAnimations()

    options = options or { go_down = true }
    if options.go_down == nil then options.go_down = true end

    local esc_command = command:gsub([[%\]], [[\\]]):gsub([[% ]], [[\ ]])
    vim.fn.execute([[vs +term\ ]] .. esc_command)

    if options.go_down then
        vim.fn.execute("norm G")
    end

    vim.g.hulvdan_EnableAnimations()
end

function launch_background(command, callback)
    vim.fn.jobstart(command, {on_exit = callback})
end

function save_files()
    vim.fn.execute(":wa")
end

function save_file_if_needed()
    if vim.bo.buftype == "" and vim.bo.modified == true then
        vim.fn.execute(":w")
    end
end

function reload_file()
    vim.fn.execute(":e")
end

function build_task_data(params)
    local close = false
    if params.close then
        close = true
    end

    local components  = {
        { "on_output_quickfix", open = true, close = close },
        "default",
    }

    if params["additional_components"] ~= nil then
        components = {
            unpack(components),
            unpack(params.additional_components),
        }
    end

    return {
        cmd = [[MSBuild .cmake\vs17\game.sln -v:minimal]],
        components = components,
    }
end

function build_task()
    return overseer.new_task(build_task_data({ close = true }))
end

function run_task()
    local launch_vs_data = {
        cmd = [[.nvim-personal\launch_vs.ahk]],
        components = {
            { "on_output_quickfix", open = true, close = true },
            "default"
        },
    }
    local build_data = build_task_data({
        close = false,
        additional_components = {
            {
                "run_after", task_names = { launch_vs_data },
                statuses = {"SUCCESS", "FAILURE"},
            },
        },
    })
    local stop_vs_data = {
        cmd = [[.nvim-personal\stop_vs.ahk]],
        components = {
            {
                "run_after", task_names = { build_data },
            },
            { "on_output_quickfix", open = true, },
            "default"
        },
    }
    return overseer.new_task(stop_vs_data)
end

function test_task()
    return overseer.new_task({
        strategy = "terminal",
        -- cmd = [[MSBuild .cmake\vs17\game.sln -v:minimal]],
        cmd = [[cmd\build.bat && cmd\run_unit_tests.bat]],
        components = {
            { "on_output_quickfix", open = true, close = true },
            "default"
        },
    })
end

function lint_task()
    return overseer.new_task({
        cmd = [[cmd\lint.bat]],
        components = {
            { "on_output_quickfix", open = true, close = true },
            "default"
        },
    })
end

-- Keyboard Shortcuts --
-- ================== --
local opts = { remap = false, silent = true }

vim.keymap.set("n", "<leader>l", function()
    save_files()
    lint_task():start()
end, opts)

vim.keymap.set("n", "<A-b>", function()
    save_files()
    build_task():start()
end, opts)


vim.keymap.set("n", "<f4>", function()
    save_files()
    build_task():start()
end, opts)

vim.keymap.set("n", "<f5>", function()
    save_files()
    run_task():start()
end, opts)

vim.keymap.set("n", "<A-t>", function()
    save_files()
    test_task():start()
    -- launch_side([[cmd\build.bat && cmd\run_unit_tests.bat]], { go_down = false })
end, opts)

vim.keymap.set("n", "<f6>", function()
    save_files()
    launch_tab([[cmd\debug.bat]])
end, opts)

vim.keymap.set("n", "<S-f6>", function()
    save_files()
    launch_tab([[cmd\debug.bat]])
end, opts)

vim.keymap.set("n", "<leader>q", function()
    vim.api.nvim_input("<M-m>:bd! #<cr>")
end)

vim.keymap.set("n", "<leader>w", function()
    save_files()

    if vim.bo.filetype == "cpp" or vim.bo.filetype == "h" then
        local view = vim.fn.winsaveview()
        todo_plugin.disable();

        local buf_path = vim.api.nvim_buf_get_name(vim.api.nvim_get_current_buf())

        launch_background([[cmd\format.bat "]] .. buf_path .. '"', function()
            reload_file()

            -- TODO(hulvdan): This horseshit of a function should take a callback
            -- and call it upon finishing the linting.
            -- We need to somehow indicate to user that file was saved and linted successfully.
            -- Mb just use a print()
            --
            -- OR
            --
            -- PREFERRABLY: Make it able to work without generating errors on fast consecutive saves
            require("lint").try_lint()

            vim.fn.winrestview(view)
            todo_plugin.enable();

            vim.api.nvim_input("mzhllhjkkj`z")  -- NOTE: for nvim-treesitter-context
        end)
    end
end, opts)

vim.keymap.set("n", "<C-S-b>", function()
    save_files()
    launch_tab([[cmd\remake_cmake.bat]])
end, opts)

require("overseer").setup({
    templates = { "builtin", "user.build_bat" },
})

-- Thanks to https://forums.handmadehero.org/index.php/forum?view=topic&catid=4&id=704#3982
-- error message formats
-- Microsoft MSBuild
vim.fn.execute([[set errorformat+=\\\ %#%f(%l\\\,%c):\ %m]])
-- Microsoft compiler: cl.exe
vim.fn.execute([[set errorformat+=\\\ %#%f(%l)\ :\ %#%t%[A-z]%#\ %m]])
-- Microsoft HLSL compiler: fxc.exe
vim.fn.execute([[set errorformat+=\\\ %#%f(%l\\\,%c-%*[0-9]):\ %#%t%[A-z]%#\ %m]])

vim.keymap.set(
    "v",
    "<C-S-U>",
    function()
        vim.api.nvim_input("<C-S-f>")

        vim.defer_fn(function()
            vim.api.nvim_input("<C-S-q><C-W>h<C-W>k")
        end, 200)
    end,
    { silent = true, remap = true }
)
