-- Workspace Settings --
-- ================== --
vim.fn.execute(":set nornu")
vim.fn.execute(":set nonumber")
-- vim.fn.execute(":set signcolumn=no")
vim.fn.execute(":set colorcolumn=")
vim.fn.execute(":set nobackup")
vim.fn.execute(":set nowritebackup")

-- Helper Functions --
-- ================ --
function launch_tab(command)
    vim.fn.execute([[term ]] .. command)
end

function launch_side(command, switch)
    local esc_command = command:gsub([[%\]], [[\\]]):gsub([[% ]], [[\ ]])
    vim.fn.execute([[vs +term\ ]] .. esc_command)

    if switch == true then
        vim.fn.execute("wincmd x")
        vim.fn.execute("wincmd l")
    end
end

function launch_background(command, callback)
    vim.fn.jobstart(command, {on_exit = callback})
end

function save_file_if_needed()
    if vim.bo.buftype == "" and vim.bo.modified == true then
        vim.fn.execute(":w")
    end
end

function reload_file()
    vim.fn.execute(":e")
end

-- Keyboard Shortcuts --
-- ================== --
local opts = { remap = false, silent = true }
vim.keymap.set("n", "<A-b>", function()
    save_file_if_needed()
    launch_side([[cmd\build.bat]], true)
end, opts)

vim.keymap.set("n", "<f4>", function()
    save_file_if_needed()
    launch_side([[cmd\build.bat]], true)
end, opts)

vim.keymap.set("n", "<f5>", function()
    save_file_if_needed()
    launch_side([[cmd\build.bat && cmd\run.bat]], true)
end, opts)

vim.keymap.set("n", "<f6>", function()
    save_file_if_needed()
    launch_tab([[cmd\debug.bat]])
end, opts)

vim.keymap.set("n", "<S-f6>", function()
    save_file_if_needed()
    launch_tab([[cmd\debug.bat]])
end, opts)

vim.keymap.set("n", "<leader>w", function()
    save_file_if_needed()

    if vim.bo.filetype == "cpp" or vim.bo.filetype == "h" then
        local view = vim.fn.winsaveview()

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
        end)
    end
end, opts)

vim.keymap.set("n", "<C-S-b>", function()
    save_file_if_needed()
    launch_tab([[cmd\remake_cmake.bat]])
end, opts)
