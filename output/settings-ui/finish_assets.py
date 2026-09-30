import pathlib
task_root=pathlib.Path('C:/Users/user1/Documents/GitHub/Unreal-MOU/output/settings-ui')
exec((task_root/'resume_key.py').read_text(encoding='utf-8-sig'))
exec((task_root/'connect.py').read_text(encoding='utf-8-sig'))
