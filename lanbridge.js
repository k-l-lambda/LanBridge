// the pm2 start script

var nrc = require('node-run-cmd');
nrc.run('.\\x64\\Release\\LanBridge.exe --port=9001', { onError(msg) { console.log(msg); } }).then(exitCode => {
	console.log("exited:", exitCode);
	process.exit(exitCode);
});
