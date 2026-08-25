const chunks = [];
process.stdin.on('data', d => chunks.push(d));
process.stdin.on('end', () => {
  try {
    const j = JSON.parse(Buffer.concat(chunks).toString());
    const toolName = j.tool_name || '';
    const input = j.tool_input || {};

    if ((toolName === 'Write' || toolName === 'Edit') && /\.csv$/i.test(input.file_path || '')) {
      const content = input.content || input.new_string || '';
      const emptyFields = (content.match(/,,/g) || []).length;
      if (emptyFields > 2) {
        console.log(JSON.stringify({
          systemMessage: '[CSV 경고] 빈 필드 ' + emptyFields + '개 감지. TSoftObjectPtr/TSubclassOf 컬럼이 비어있으면 Reimport 시 None으로 덮어씌워짐'
        }));
      }
    }
  } catch (e) {}
});
