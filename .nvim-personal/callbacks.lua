vim.api.nvim_create_autocmd({ "BufEnter" }, {
    pattern = "*",
    once = false,
    callback = function()
        vim.fn.execute("set signcolumn=no")
        if vim.bo.filetype == "md" then
            return
        end

        if vim.bo.filetype ~= nil and vim.bo.filetype ~= "" then
            vim.fn.execute("set nowrap")
        end
    end,
})
