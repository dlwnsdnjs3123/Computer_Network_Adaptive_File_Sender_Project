# Computer_Network_Adaptive_File_Sender_Project

컴퓨터네트워크 수업에서 진행한 파일 전송 sender 프로젝트입니다. 오류가 발생할 수 있는 채널 환경에서 CRC 기반 프레이밍, ACK/NAK 재전송, 적응형 payload 크기 조절을 구현했습니다.

## 개요

- 제공된 네트워크 시뮬레이터 인터페이스 위에서 reliable sender 구현
- 2바이트 payload 길이 헤더와 4바이트 CRC trailer를 갖는 frame 구성
- `NAK` 응답 시 동일 데이터 재전송
- ACK/NAK 결과를 바탕으로 채널 상태 추정
- 전송 효율을 높이기 위한 adaptive payload sizing 적용

## 파일 구성

- `sender.cc`: sender 핵심 구현
- `netsim.h`: 시뮬레이터 인터페이스
- `netsim_lib.cc`: 시뮬레이터와의 입출력 처리 보조 코드
- `Makefile`: 빌드 설정

## 동작 방식

sender는 입력 파일을 바이너리 모드로 읽고, 아직 ACK되지 않은 데이터를 pending buffer에 유지합니다. 각 전송 단위는 다음과 같은 frame으로 구성됩니다.

1. 2바이트 big-endian payload 크기
2. payload 데이터
3. 4바이트 CRC

전송 후에는 시뮬레이터로부터 `ACK` 또는 `NAK`를 받습니다. 이 구현은 고정 payload 크기를 사용하지 않고, 누적된 ACK/NAK 결과를 바탕으로 채널 오류율에 대한 내부 추정을 업데이트합니다. 이후 미리 정의한 payload 후보군 중에서 다음 전송 크기를 선택해, 채널이 안정적일 때는 더 큰 payload를 사용하고 오류가 잦을 때는 보수적으로 동작하도록 만들었습니다.

## 기술 포인트

- CRC32 스타일의 frame 검증 값 계산
- 전송 결과 기반 posterior 업데이트
- 파일 입력, frame 생성, payload 선택 로직 분리
- partial write, read 실패, 시뮬레이터 통신 오류 처리

## 빌드

```bash
make
```

또는 다음 명령으로 직접 빌드할 수 있습니다.

```bash
g++ -O2 -std=c++17 -o sender sender.cc netsim_lib.cc
```

## 실행 메모

이 프로젝트는 원래 수업에서 제공된 외부 시뮬레이터 프로세스와 함께 실행하는 과제였습니다. 따라서 공개 저장소에는 구현 코드와 인터페이스만 포함했고, 과제 배포용 실행 파일이나 대용량 테스트 자산은 제외했습니다.

## 포트폴리오 메모

원본 제출물에서 불필요한 샘플 파일과 실행 파일을 제거하고, 구현 의도가 잘 드러나도록 정리한 공개용 버전입니다.
