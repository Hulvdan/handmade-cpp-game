vim.keymap.set("n", "\\b", function() 
    vim.fn.execute('term cmd\\build.bat')
end)

vim.keymap.set("n", "<f5>", function() 
    vim.fn.execute('term cmd\\build.bat && cmd\\run.bat')
end)

vim.keymap.set("n", "<f6>", function() 
    vim.fn.execute('term cmd\\build.bat && cmd\\debug.bat')
end)

