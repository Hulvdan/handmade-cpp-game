vim.api.nvim_create_autocmd({ "BufEnter" }, {
    pattern = "*",
    once = false,
    callback = function()
        -- Включаем wrap для markdown файлов.
        if vim.bo.filetype == "markdown" then
            vim.fn.execute("set wrap")
            return
        end

        if vim.bo.filetype ~= nil and vim.bo.filetype ~= "" then
            vim.fn.execute("set nowrap")
        end
    end,
})
