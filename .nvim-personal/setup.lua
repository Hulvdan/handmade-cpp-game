-- Workspace Settings --
-- ================== --
vim.fn.execute(":set nornu")
vim.fn.execute(":set nonumber")

-- Keyboard Shortcuts --
-- ================== --
local opts = { remap = false, silent = true }
vim.keymap.set("n", "\\b", function()
    vim.fn.execute(":w")
    vim.fn.execute("vs +term\\ cmd\\\\build.bat")
    vim.fn.execute("wincmd x")
    vim.fn.execute("wincmd l")
end, opts)

vim.keymap.set("n", "<f5>", function()
    vim.fn.execute(":w")
    vim.fn.execute("term cmd\\build.bat && cmd\\run.bat")
    vim.fn.execute("wincmd x")
    vim.fn.execute("wincmd l")
end, opts)

vim.keymap.set("n", "<f6>", function()
    vim.fn.execute(":w")
    vim.fn.execute("term cmd\\build.bat && cmd\\debug.bat")
end, opts)

vim.keymap.set("n", "<C-\\>", function()
    vim.fn.execute("vs")
end, opts)
vim.keymap.set("n", "<C-S-\\>", function()
    vim.fn.execute("vs +echo 1")
    vim.fn.execute("wincmd l")
end, opts)

