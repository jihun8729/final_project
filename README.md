<h1> 사용 방법</h1>
<h2>Sender</h2>
1. make <br>
2. 실행

![1](https://github.com/user-attachments/assets/044e488b-01bc-425f-ba9b-5cc95260f0e3)
<br>./p2p -p <자신의 포트 번호> -s -n <리시버 수> -f <파일 이름> -g <세그먼트 크기>

<h2>Receiver</h2>
1. make <br>
2. 실행
<br>./p2p -p <자신의 포트 번호> -r -a <서버의 ip> <서버의 포트 번호>
