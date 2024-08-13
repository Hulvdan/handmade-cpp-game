bufenter_function = function()
    -- Включаем wrap для markdown файлов.
    if vim.bo.filetype == "markdown" then
        vim.fn.execute("set wrap")
        return
    end

    local extension = vim.fn.expand("%"):match("^.+(%..+)$") -- example: `.log`

    if extension == ".log" then
        vim.fn.execute("setlocal tabstop=2")
        return
    end

    if vim.bo.filetype ~= nil and vim.bo.filetype ~= "" then
        vim.fn.execute("set nowrap")
    end
end

vim.g.hulvdan_bufenter_callbacks_function = bufenter_function

-- Делаем так, чтобы callback был установлен лишь один раз.
if not vim.g.hulvdan_bufenter_callbacks_were_set then
    vim.g.hulvdan_bufenter_callbacks_were_set = true

    vim.api.nvim_create_autocmd({ "BufEnter" }, {
        pattern = "*",
        once = false,
        callback = function()
            if vim.g.hulvdan_bufenter_callbacks_function ~= nil then
                vim.g.hulvdan_bufenter_callbacks_function()
            end
        end,
    })
end
