pipeline {
  agent { 
    docker { 
      label 'docker-windows'
      image 'mcr.microsoft.com/dotnet/sdk:8.0-windowsservercore-ltsc2022'
    }
  }
  
  stages {
	stage('build') {
	  steps {
		bat "dotnet nuget locals all --clear"
		bat "dotnet build -c release -warnaserror"
	  }
	}

	stage('publish') {
	  when { branch 'release/*' }
	  steps {
		bat "dotnet publish TileServer\\TileServer.csproj -f net8.0 -r win-x64 -c release --no-self-contained -o ${env.WORKSPACE}\\publish\\TileServer"
		uploadProgetPackage artifacts: "[publish]\\TileServer\\**\\*", feedName: 'S2.Next', groupName: '', packageName: 'TileServer', version: "${env.BRANCH_NAME.split('/')[1]}.${env.BUILD_NUMBER}"
	  }
	}
  }

  post {
    failure {
	  emailext recipientProviders: [[$class: 'DevelopersRecipientProvider'], [$class: 'RequesterRecipientProvider']], subject: "JENKINS ${currentBuild.currentResult} : ${env.JOB_NAME}", body: '''${SCRIPT, template="groovy-html.template"}''', mimeType: 'text/html'
	}
	always {
	    powershell "remove-item ${env.WORKSPACE}\\* -recurse -force"
	}
  }
}

