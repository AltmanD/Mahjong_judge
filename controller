from subprocess import *
import json

# path of judge executable file, you need to get from the game wiki page and compile it first
judge_path = '/Users/syys/software/mj_judge/Chinese-Standard-Mahjong/judge/judge'

bot_path = ['', '', '', '']
for j in range(4):
    # replace bot_path with your bot program as '/path/to/bot/bot.exe'
    bot_path[j] = 'python /Users/syys/PycharmProjects/majong_gb/v2/bot.py'

# initial file for the judge
with open('./begin.json', 'r') as f:
    judge_input_json = json.load(f)


def send_input_to_bot(input_json, bot_id):
    input_str = json.dumps(input_json)
    bot_proc = Popen(bot_path[bot_id], stdin=PIPE, stdout=PIPE, shell=True)
    bot_proc.stdin.write(input_str.encode('UTF-8'))
    bot_proc.stdin.close()
    ret_bytes = bot_proc.stdout.readlines()
    bot_proc.terminate()
    ret_str = ''
    for tstr in ret_bytes:
        real_str = tstr.decode()
        real_str = real_str.replace('\t', '')
        ret_str += real_str.replace('\n', '')
    bot_output_json = json.loads(ret_str)
    return bot_output_json


def send_input_to_judge(input_json):
    input_str = json.dumps(input_json)
    judge_proc = Popen(judge_path, stdin=PIPE, stdout=PIPE)
    judge_proc.stdin.write(input_str.encode('UTF-8'))
    judge_proc.stdin.close()
    ret_bytes = judge_proc.stdout.readlines()
    judge_proc.terminate()
    ret_str = ''
    for tstr in ret_bytes:
        real_str = tstr.decode()
        real_str = real_str.replace('\t', '')
        ret_str += real_str.replace('\n', '')
    judge_output_json = json.loads(ret_str)
    return judge_output_json


player_input = [{}, {}, {}, {}]
for p in range(4):
    player_input[p]['requests'] = []
    player_input[p]['responses'] = []
step = 0
while True:
    step += 1
    print('step: ', step)
    judge_output_json = send_input_to_judge(judge_input_json)
    command = judge_output_json['command']
    if command == 'finish':
        print('finish')
        break
    judge_input_json['log'].append({'output': judge_output_json})
    po = judge_output_json['content']
    for key in po:
        tplayer = int(key)
        player_input[tplayer]['requests'].append(po[key])
        print(po[key])
    judge_input_json['log'].append({})
    for i in range(4):
        bot_output_json = send_input_to_bot(player_input[i], i)
        print(bot_output_json)
        player_input[i]['responses'].append(bot_output_json['response'])
        judge_input_json['log'][-1][str(i)] = {'response': bot_output_json['response'], 'verdict': 'OK'}
    print(judge_input_json['log'][-1])
    print(len(judge_input_json['log']))
