### git

1.工作区、暂存区、分支的具体含义

工作区就是git init之后的目录。

从工作区add指令后就会进入stage区

stage区执行commit指令就会进入master区

2.git版本回退和撤销修改

版本日志
	
	查看日志 git log
	简化一次提交为一行版本的查看日志 git log --pretty=oneline
	另一种查看日志的方式(可以看到commit_id) git reflog
	带有分支信息的日志查看方式 git log --graph --pretty=oneline --abbrev-commit
	
版本回退

	回退到上一个版本 git reset --hard HEAD~
	回退到上上一个版本 git reset --hard HEAD~~
	回退到上10个版本 git reset --hard HEAD~10
	回退到指定版本号 git reset --hard <commit_id>
	

撤销修改
		
	撤销工作区的修改 git checkout --file
	注：如果暂存区有内容，撤销成和暂存区一致，否则和分支保持一致
	撤销stage区 git reset HEAD <file>
	
	
### 分支	
创建
	
	创建分支 git branch <branchname>
	切换分支 git checkout <branchname>
	创建分支并切换到该分支 git checkout -b <branchname>
	删除分支 git branch -d <branchname>
	强制删除 git branch -D <branchname>
合并
		
	Fast Forward mode[合并后没有记录]  git merge branchname
	递归策略式合并[合并后有记录] git merge --no-ff -m "merge lisa branch 2 master branch" <branchname>		
冲突
		
	正常处理conflict，修改后，add，再commit
	快速修改冲突 git checkout ours file [接受本地的]
	git checkout theirs file [接受服务器的]

储存

	储存 git stash	
	查看储存的列表 git stash list
	恢复方法 1.git stash apply 不会删除储存的东西
			2.git stash pop 删除储存的东西
	恢复固定的储存 git stash apply stash@{0}		
多人协作

	首先，可以试图用git push origin branch-name推送自己的修改；
	如果推送失败，则因为远程分支比你的本地更新，需要先用git pull试图合并；

	如果合并有冲突，则解决冲突，并在本地提交；

	没有冲突或者解决掉冲突后，再用git push origin branch-name推送就能成功！

	如果git pull提示“no tracking information”，
	则说明本地分支和远程分支的链接关系没有创建，
	用命令 git branch --set-upstream <branch-name> origin/<branch-name>
	

标签 tag （目的是为了更好回退，其实tag也是指向某一次commit提交的）

	创建标签 git tag <tagname>
	查看tag git tag
	针对某一次commit提交创建tag git tag v1.0 <commit_id>
	查看tag具体是commit信息 git show <tagname>
	创建标签的时候添加一个message  git tag -a v0.1 -m "version 0.1 released" <commit_id>
	
	删除标签 git tag -d v1.0
	推送tag到远程分支 git push origin <tagname>
	一次性全部推送本地tag到远程分支 git push origin --tags
	删除远程tag git push origin :refs/tags/<tagname>
	
别名

	将st替换status  git config --global alias.st status
	
	好用的配置 git config --global alias.lg "log --color --graph --pretty=format:'%Cred%h%Creset -%C(yellow)%d%Creset %s %Cgreen(%cr) %C(bold blue)<%an>%Creset' --abbrev-commit"

			
		
		