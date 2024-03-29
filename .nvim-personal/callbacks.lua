vim.api.nvim_create_autocmd({"BufEnter"}, {
    pattern = "*",
    once = false,
    callback = function()
        if vim.bo.filetype ~= nil and vim.bo.filetype ~= "" then
            vim.fn.execute("set nowrap")
        end
    end
})
