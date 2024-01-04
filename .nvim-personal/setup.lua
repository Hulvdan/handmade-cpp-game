-- Workspace Settings --
-- ================== --
vim.fn.execute(":set nornu")
vim.fn.execute(":set nonumber")
-- vim.fn.execute(":set signcolumn=no")

-- Helper Functions --
-- ================ --
function launch_tab(command)
    local command = [[cmd\build.bat && cmd\run.bat]]
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

function reload_file()
    vim.fn.execute(":e")
end

-- Keyboard Shortcuts --
-- ================== --
local opts = { remap = false, silent = true }
vim.keymap.set("n", "\\b", function()
    -- vim.fn.execute(":w")
    launch_side([[cmd\build.bat]], true)
end, opts)

vim.keymap.set("n", "<f5>", function()
    -- vim.fn.execute(":w")
    launch_side([[cmd\build.bat && cmd\run.bat]], true)
end, opts)

vim.keymap.set("n", "<f6>", function()
    -- vim.fn.execute(":w")
    launch_tab([[cmd\build.bat && cmd\debug.bat]])
end, opts)

vim.keymap.set("n", "<S-f6>", function()
    -- vim.fn.execute(":w")
    launch_tab([[cmd\debug.bat]])
end, opts)

vim.keymap.set("n", "<C-S-f>", function()
    vim.fn.execute(":w")
    launch_background([[cmd\format.bat]], reload_file)
end, opts)
