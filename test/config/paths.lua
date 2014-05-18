local module = {}

module.include_paths = {}
for p in ("/home/zyx/a.a/Proj/c/neovim/build/config;/home/zyx/a.a/Proj/c/neovim/src;/home/zyx/a.a/Proj/c/neovim/src/generated;/usr/include;/usr/include;/usr/include/luajit-2.0" .. ";"):gmatch("[^;]+") do
  table.insert(module.include_paths, p)
end

module.test_include_path = "/home/zyx/a.a/Proj/c/neovim/build/test/includes/post"
module.test_libnvim_path = "/home/zyx/a.a/Proj/c/neovim/build/src/nvim/libnvim-test.so"
table.insert(module.include_paths, "/home/zyx/a.a/Proj/c/neovim/build/include")

return module
